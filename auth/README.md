# Feature: Auth (ONVIF WS-Security Authentication)

Module này chịu trách nhiệm mã hóa và xác thực tài khoản/mật khẩu camera theo chuẩn **ONVIF WS-Security UsernameToken (PasswordDigest)**.

---

## 1. Cấu trúc thư mục

```text
auth/
├── include/
│   ├── sha1.h          # Thuật toán băm SHA-1 (RFC 3174)
│   ├── base64.h        # Mã hóa Base64
│   └── ws_security.h   # Sinh SOAP Security Header với PasswordDigest
├── source/
│   ├── sha1.cpp
│   ├── base64.cpp
│   └── ws_security.cpp
└── README.md
```

---

## 2. Chuẩn xác thực ONVIF (WS-UsernameToken)

Theo đặc tả ONVIF Core Specification, mật khẩu không bao giờ được gửi dạng plaintext. Thay vào đó, camera sử dụng công thức băm một chiều:

$$\text{PasswordDigest} = \text{Base64}\Big(\text{SHA-1}\big(\text{RawNonce} + \text{CreatedUTC} + \text{Password}\big)\Big)$$

Trong đó:
1. `RawNonce`: 16 bytes ngẫu nhiên ngăn chặn tấn công replay attack.
2. `CreatedUTC`: Thời gian hiện tại theo giờ quốc tế UTC định dạng ISO 8601 (ví dụ `2026-09-26T07:30:00Z`).
3. `Password`: Mật khẩu của tài khoản camera.

---

## 3. Hướng dẫn sử dụng API

### `WsSecurity::buildSecurityHeader(username, password)`
Tự động sinh đoạn XML `<wsse:Security>` hoàn chỉnh để gắn vào thẻ `<s:Header>` của bản tin SOAP:

```cpp
#include "ws_security.h"
#include <iostream>

std::string secHeader = Eyerus::WsSecurity::buildSecurityHeader("admin", "Admin@123");
std::cout << secHeader << std::endl;
```

**Kết quả XML mẫu được tạo ra:**
```xml
    <wsse:Security xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
      <wsse:UsernameToken>
        <wsse:Username>admin</wsse:Username>
        <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">hN8s...=</wsse:Password>
        <wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">qW3x...==</wsse:Nonce>
        <wsu:Created>2026-09-26T07:30:00Z</wsu:Created>
      </wsse:UsernameToken>
    </wsse:Security>
```

