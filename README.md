Installation Guide
==============

The only requirements to build this are:
A working C++ compiler, capable of compiling C++17
CMake
SDL3 (this is probably the hardest to install)

To install sdl3 
on mac: `brew install sdl3`
on linux: in some package managers
on windows: also idk, probably install it here? https://github.com/libsdl-org/SDL/releases/tag/release-3.2.8

Then, just run 

```
git clone https://github.com/KevinH15291/Game-Boy-Emulator
cd Game-Boy-Emulator
cmake .
make
```


Despite the cmake project's name, it does not support color features yet.

You can then run a rom by doing `./GBC` and then entering the path to your rom. Please make sure it uses the MB1, MB3, or no memory bank controller, as those are the only ones supported so far.

Mapping
A: A
B: S
Start: Z
Select: X

Some images:
![image](https://github.com/user-attachments/assets/7fe80fb9-3d9b-4772-95e2-ff6a724c9a89)

![image](https://github.com/user-attachments/assets/df287824-9dcd-4e75-878e-453c6f37ebd3)


![image](https://github.com/user-attachments/assets/06c53c0a-8065-4ae2-84d5-715ba7d5ead6)

older:
https://github.com/user-attachments/assets/c27fa8ee-212c-405e-b31f-c1452a69a391

growing pains :|
<img width="752" alt="image" src="https://github.com/user-attachments/assets/d98fe28b-0d0e-414b-a6a9-6d18887bf367" />



