# Modern Durak Server
Server which allows [Modern Durak Clients](https://github.com/werto87/game_01_client) to connect.

## Easy way to run Modern Durak Server with using Docker
- get the docker image by running 'docker pull ghcr.io/werto87/game_01_server/modern-durak-server:latest'

## How to build the Mordern Durak Server from source
Note: 
- Only tested on linux
### Prerequisite
- Conan Package Manager - "https://conan.io/downloads.html"
- Cmake - "https://cmake.org/download/"
- Clang which supports c++ concepts-ts and coroutine-ts (atleast partial) - tested with clang 13
- LLVM libstd++ implementation libc++
### Test Prerequisite
Test the installation by running the following commands:
- 'cmake --version'
- 'conan --version'
- 'clang --version'

If you get something like "command not found" please check your installation.

### Setup Conan Package Manager
- Add the remote with some packages needed to build the server 'conan remote add gitlab https://gitlab.com/api/v4/projects/27217743/packages/conan'
- Create a profile for clang
  - Create a profile and print the path 'conan profile new clang'
  - Use the path from step aboth and open the profile
  - Fill in the Profile  
[settings]  
os=Linux  
arch=x86_64  
compiler=clang  
compiler.version=13  
compiler.libcxx=libc++  
[env]  
CC=Path to clang  
CXX=Path to clang++  

### Build the Modern Durak Server
- Clone the repo 'git clone https://github.com/werto87/game_01_server.git'
- Change into cloned repo 'cd game_01_server/'
- Create build folder and change into it 'mkdir build && cd build'
- Get the dependencies 'conan install .. --profile clang -s build_type=Debug --build missing'
- Configure cmake 'cmake  .. -DCMAKE_BUILD_TYPE=Debug -D CMAKE_C_COMPILER=/usr/bin/clang -D CMAKE_CXX_COMPILER=/usr/bin/clang++'
- Build Modern Durak Server 'cmake --build .'
## How to run Modern Durak Server
Run Modern Durak Server on default port which is 55555 './bin/project'  
Run Modern Durak Server on 44444 './bin/project 44444'  
Note: Currently the Client tries to connect to localhost:55555 
