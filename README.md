# VTA-Simulator

A simple SystemC simulation project in C++ using **Visual Studio Code** and **vcpkg** on Windows.

The project demonstrates:

- Using **SC_CTHREAD** for clocked modules
- Communication between modules via **sc_signal**
- Logging outputs to **console (cout)** and **files (fstream)**
- Easy integration with **vcpkg-installed SystemC**

---

## **Project Structure**



---

## **Requirements**

- Windows 10/11
- Visual Studio 2022 Build Tools (x64)
- CMake ≥ 3.15
- vcpkg package manager
- SystemC installed via vcpkg:

```bash
vcpkg install systemc:x64-windows
```

- cppzmq/zeromq installed via vcpkg:

```bash
vcpkg install cppzmq:x64-windows
vcpkg install zeromq:x64-windows
```

- json packages installed via vcpkg:

```bash
vcpkg install json-schema-validator:x64-windows
vcpkg install nlohmann-json:x64-windows
```

- magic-enum installed via vcpkg:

```bash
vcpkg install magic-enum
```
