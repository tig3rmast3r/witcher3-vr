# Third-party notices

Witcher 3 VR contains or is derived from third-party software. The project's
MIT license applies only to original Witcher 3 VR code. Third-party components
remain governed by their respective terms.

The dependency setup script retrieves exact upstream revisions. Dependency
source trees are not committed to this repository and must not be treated as
being relicensed by Witcher 3 VR.

## MIT-licensed components

The following components are incorporated or adapted under the MIT License:

- **DLSSTweaks**
  - Project: https://github.com/emoose/DLSSTweaks
  - Reference commit used for the ForceDLAA approach:
    `1d2fddbe3d1e8f403f795afc17e7db239a83f6a2`
  - Copyright (c) 2023 emoose
  - License: MIT
- **Microsoft DirectX-Headers**
  - Project: https://github.com/microsoft/DirectX-Headers
  - Pinned revision: `2c305c16da8a4450db8d7f1e7d8d014c8bc665ee`
  - Copyright (c) Microsoft Corporation
  - License: MIT
- **Khronos OpenXR headers**
  - Project: https://github.com/KhronosGroup/OpenXR-SDK
  - Pinned revision: `5267613edf3d937e3d77556a106a65c2f82b25c6`
  - Copyright 2017-2026 The Khronos Group Inc.
  - Header SPDX license: `Apache-2.0 OR MIT`
  - Witcher 3 VR relies on the MIT option for the generated OpenXR headers.
- **Khronos OpenXR Loader 1.0.22**
  - Project: https://github.com/KhronosGroup/OpenXR-SDK
  - Pinned revision: `458984d7f59d1ae6dc1b597d94b02e4f7132eaba`
  - Copyright 2017-2020 The Khronos Group Inc.
  - Loader SPDX license: `Apache-2.0 OR MIT`
  - The release package includes the 64-bit loader built from this revision
    under the MIT option.

The MIT License text applying to the components above is reproduced here:

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES, OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT, OR OTHERWISE, ARISING FROM,
> OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Development references

[REFramework](https://github.com/praydog/REFramework) and
[UEVR](https://github.com/praydog/UEVR) by praydog were consulted as DX12,
hooking, OpenXR, and VR architecture references during initial development.
Their source trees are not compiled, committed, or distributed as part of
Witcher 3 VR.

Early private builds obtained MinHook through REFramework's dependency tree.
The public dependency setup instead retrieves MinHook directly from its
official upstream repository under the license reproduced below.

## MinHook and Hacker Disassembler Engine

- Project: https://github.com/TsudaKageyu/minhook
- Pinned revision: `98b74f1fc12d00313d91f10450e5b3e0036175e3`

MinHook - The Minimalistic API Hooking Library for x64/x86  
Copyright (C) 2009-2017 Tsuda Kageyu.  
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.

### Hacker Disassembler Engine 32 C

Copyright (c) 2008-2009, Vyacheslav Patkov.  
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

### Hacker Disassembler Engine 64 C

Copyright (c) 2008-2009, Vyacheslav Patkov.  
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## NVIDIA NGX SDK headers

- Source repository: https://github.com/NVIDIA-RTX/Streamline
- Pinned revision: `e8aaa6eaac968711fb62473d4ae8256dde20919b`
- Governing license:
  https://github.com/NVIDIA-RTX/Streamline/blob/main/external/ngx-sdk/license.txt

The dependency setup retrieves NVIDIA NGX SDK interface headers from the
official Streamline repository. NVIDIA SDK materials are not licensed under
the Witcher 3 VR MIT License and must not be redistributed as a standalone
SDK.

Required NVIDIA notice:

> This software contains source code provided by NVIDIA Corporation.

No NVIDIA game DLL or standalone SDK package is distributed by Witcher 3 VR.
Users must rely on the DLSS/NGX runtime supplied with their legally installed
game and compatible NVIDIA driver.

## CD PROJEKT RED fan content

Witcher 3 VR is an unofficial fan work and is not approved or endorsed by
CD PROJEKT RED. It does not distribute game assets and requires a legally
obtained copy of The Witcher 3: Wild Hunt.

Fan Content Guidelines:
https://www.cdprojektred.com/en/fan-content
