# its debian based
FROM silkeh/clang:12

##INSTALL

COPY . /server

RUN apt update

RUN apt-get -y install python3-pip

RUN pip3 install conan

WORKDIR /server

RUN conan profile new clang

RUN echo "[settings]\nos=Linux\narch=x86_64\ncompiler=clang\ncompiler.version=12\ncompiler.libcxx=libc++\n[env]\nCC=/usr/local/bin/clang\nCXX=/usr/local/bin/clang++" > /root/.conan/profiles/clang

RUN conan remote add gitlab https://gitlab.com/api/v4/projects/27217743/packages/conan

RUN rm -rf build ; mkdir build && cd build && conan install .. --profile clang -s build_type=Release --build missing && cmake  .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1  -D CMAKE_C_COMPILER=/usr/local/bin/clang -D CMAKE_CXX_COMPILER=/usr/local/bin/clang++ && cmake --build . && cd ..

CMD [ "/server/build/bin/project"]


#TODO do not forget to add a way to provide secrets maybe with virtual files