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

### Test Prerequisite
Test the installation by running the following commands:
- 'cmake --version'
- 'conan --version'

If you get something like "command not found" please check your installation.

### Setup Conan Package Manager
- Add the remote with some packages needed to build the server 'conan remote add gitlab https://gitlab.com/api/v4/projects/27217743/packages/conan'

### Build the Modern Durak Server
- Clone the repo 'git clone https://github.com/werto87/game_01_server.git'
- Change into cloned repo 'cd game_01_server/'
- Create build folder and change into it 'mkdir build && cd build'
- Get the dependencies 'conan install .. --build missing'
- Configure cmake 'cmake ..'
- Build Modern Durak Server 'cmake --build .'
## How to run Modern Durak Server
Run Modern Durak Server on default port which is 55555 './bin/project'  
Run Modern Durak Server on 44444 './bin/project 44444'  
Note: Currently the Client tries to connect to localhost:55555 
