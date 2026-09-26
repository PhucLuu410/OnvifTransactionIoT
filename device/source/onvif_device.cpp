#include "onvif_device.h"
#include "http_client.h"
#include "ws_security.h"
#include "pugixml.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>

namespace Eyerus
{

    namespace
    {

        bool localNameEquals(const char *nodeName, const std::string &targetName)
        {
            if (!nodeName)
                return false;
            std::string name(nodeName);
            size_t colon = name.find(':');
            std::string local = (colon != std::string::npos) ? name.substr(colon + 1) : name;
            return (local == targetName);
        }

        pugi::xml_node findChildByLocalName(pugi::xml_node parent, const std::string &localName)
        {
            for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling())
            {
                if (localNameEquals(child.name(), localName))
                {
                    return child;
                }
            }
            return pugi::xml_node();
        }

        void findNodesByLocalName(pugi::xml_node root, const std::string &targetName, std::vector<pugi::xml_node> &outList)
        {
            if (localNameEquals(root.name(), targetName))
            {
                outList.push_back(root);
            }
            for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
            {
                findNodesByLocalName(child, targetName, outList);
            }
        }

    } // anonymous namespace

    void PingResult::printSummary() const
    {
        std::cout << "==================================================\n";
        std::cout << "               KET QUA PING CAMERA                \n";
        std::cout << "==================================================\n";
        std::cout << " Trang thai mang   : " << (isReachable ? "ONLINE [OK]" : "OFFLINE / UNREACHABLE [FAILED]") << "\n";
        if (isReachable)
        {
            std::cout << " Do tre (Latency)  : " << latencyMs << " ms\n";
            std::cout << " Ma HTTP Status    : " << httpStatusCode << "\n";
            std::cout << " Xac thuc Tai khoan: " << (isAuthenticated ? "DANG NHAP THANH CONG [OK]" : "DANG NHAP THAT BAI [FAILED]") << "\n";

            if (isAuthenticated)
            {
                std::cout << " Hang san xuat     : " << manufacturer << "\n";
                std::cout << " Model camera      : " << model << "\n";
                std::cout << " Firmware Version  : " << firmwareVersion << "\n";
                std::cout << " Serial Number     : " << serialNumber << "\n";
                if (!hardwareId.empty())
                {
                    std::cout << " Hardware ID       : " << hardwareId << "\n";
                }
            }
            else
            {
                std::cout << " Chi tiet loi      : " << errorMessage << "\n";
            }
        }
        else
        {
            std::cout << " Chi tiet loi      : " << errorMessage << "\n";
        }
        std::cout << "==================================================\n";
    }

    bool OnvifDevice::getDeviceInformation(
        const std::string &xaddr,
        const std::string &username,
        const std::string &password,
        PingResult &outResult,
        int timeoutMs)
    {
        std::string secHeader = WsSecurity::buildSecurityHeader(username, password);

        std::ostringstream soapReq;
        soapReq << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
                << "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
                << "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n";

        if (!secHeader.empty())
        {
            soapReq << "  <s:Header>\r\n"
                    << secHeader
                    << "  </s:Header>\r\n";
        }

        soapReq << "  <s:Body>\r\n"
                << "    <tds:GetDeviceInformation/>\r\n"
                << "  </s:Body>\r\n"
                << "</s:Envelope>";

        HttpResponse httpResp;
        bool success = HttpClient::postSoap(xaddr, soapReq.str(), httpResp, timeoutMs);

        outResult.latencyMs = httpResp.roundTripMs;
        outResult.httpStatusCode = httpResp.statusCode;

        if (!success)
        {
            outResult.isReachable = false;
            outResult.isAuthenticated = false;
            outResult.errorMessage = "Ket noi that bai hoac timeout den endpoint " + xaddr;
            return false;
        }

        outResult.isReachable = true;

        // Kiem tra ma HTTP 401 Unauthorized
        if (httpResp.statusCode == 401)
        {
            outResult.isAuthenticated = false;
            outResult.errorMessage = "HTTP 401: Sai ten dang nhap hoac mat khau.";
            return false;
        }

        // Parse XML body
        pugi::xml_document doc;
        pugi::xml_parse_result parseResult = doc.load_buffer(httpResp.body.c_str(), httpResp.body.length());
        if (!parseResult)
        {
            outResult.isAuthenticated = false;
            outResult.errorMessage = "Phan hoi tu camera khong phai XML hop le.";
            return false;
        }

        // Kiem tra xem co SOAP Fault (NotAuthorized) hay khong
        std::vector<pugi::xml_node> faults;
        findNodesByLocalName(doc, "Fault", faults);
        if (!faults.empty())
        {
            outResult.isAuthenticated = false;
            std::string lowerBody = httpResp.body;
            std::transform(lowerBody.begin(), lowerBody.end(), lowerBody.begin(), ::tolower);
            if (lowerBody.find("authorized") != std::string::npos || lowerBody.find("auth") != std::string::npos)
            {
                outResult.errorMessage = "Xac thuc that bai (Sai username hoac password).";
            }
            else
            {
                outResult.errorMessage = "Camera tra ve SOAP Fault: Thiet bi tu choi yeu cau.";
            }
            return false;
        }

        std::vector<pugi::xml_node> devInfoNodes;
        findNodesByLocalName(doc, "GetDeviceInformationResponse", devInfoNodes);

        if (devInfoNodes.empty())
        {
            outResult.isAuthenticated = false;
            outResult.errorMessage = "Khong tim thay GetDeviceInformationResponse trong phan hoi.";
            return false;
        }

        pugi::xml_node infoNode = devInfoNodes[0];
        pugi::xml_node mfg = findChildByLocalName(infoNode, "Manufacturer");
        pugi::xml_node mdl = findChildByLocalName(infoNode, "Model");
        pugi::xml_node fwv = findChildByLocalName(infoNode, "FirmwareVersion");
        pugi::xml_node ser = findChildByLocalName(infoNode, "SerialNumber");
        pugi::xml_node hid = findChildByLocalName(infoNode, "HardwareId");

        outResult.isAuthenticated = true;
        outResult.manufacturer = mfg ? mfg.child_value() : "Unknown";
        outResult.model = mdl ? mdl.child_value() : "Unknown";
        outResult.firmwareVersion = fwv ? fwv.child_value() : "Unknown";
        outResult.serialNumber = ser ? ser.child_value() : "Unknown";
        outResult.hardwareId = hid ? hid.child_value() : "";

        return true;
    }

    bool OnvifDevice::getSystemDateAndTime(
        const std::string &xaddr,
        std::string &outDateString,
        int timeoutMs)
    {
        std::string soapReq =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
            "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\r\n"
            "  <s:Body>\r\n"
            "    <tds:GetSystemDateAndTime/>\r\n"
            "  </s:Body>\r\n"
            "</s:Envelope>";

        HttpResponse httpResp;
        if (!HttpClient::postSoap(xaddr, soapReq, httpResp, timeoutMs))
        {
            return false;
        }

        pugi::xml_document doc;
        if (!doc.load_buffer(httpResp.body.c_str(), httpResp.body.length()))
        {
            return false;
        }

        std::vector<pugi::xml_node> dtNodes;
        findNodesByLocalName(doc, "GetSystemDateAndTimeResponse", dtNodes);
        if (!dtNodes.empty())
        {
            outDateString = "OK";
            return true;
        }

        return false;
    }

    PingResult OnvifDevice::ping(
        const std::string &xaddr,
        const std::string &username,
        const std::string &password,
        int timeoutMs)
    {
        PingResult res;

        // Neu co tai khoan: goi GetDeviceInformation voi WS-Security
        if (!username.empty())
        {
            getDeviceInformation(xaddr, username, password, res, timeoutMs);
            return res;
        }

        // Neu khong co tai khoan: goi GetSystemDateAndTime de kiem tra tinh trang song/chet
        std::string dt;
        bool ok = getSystemDateAndTime(xaddr, dt, timeoutMs);
        res.isReachable = ok;
        res.isAuthenticated = false;
        if (!ok)
        {
            res.errorMessage = "Khong the ket noi den camera (Khong phan hoi).";
        }
        else
        {
            res.errorMessage = "Camera phan hoi tot (Chua kiem tra xac thuc vi khong truyen Username).";
        }

        return res;
    }

} // namespace Eyerus
