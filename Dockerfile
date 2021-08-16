# its debian based
#FROM silkeh/clang:12
#TODO made it small for tests to deploy it
FROM busybox:glibc


# COPY . /server

# ##INSTALL conan
# RUN apt update

# RUN apt-get -y install python3-pip

# RUN pip3 install conan

# RUN conan profile new clang

# RUN echo "[settings]\nos=Linux\narch=x86_64\ncompiler=clang\ncompiler.version=12\ncompiler.libcxx=libc++\n[env]\nCC=/usr/local/bin/clang\nCXX=/usr/local/bin/clang++" > /root/.conan/profiles/clang

# RUN conan remote add gitlab https://gitlab.com/api/v4/projects/27217743/packages/conan

# # Build the server 

# RUN cd /server && rm -rf build ; mkdir build && cd build && conan install .. --profile clang -s build_type=Release --build missing && cmake  .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1  -D CMAKE_C_COMPILER=/usr/local/bin/clang -D CMAKE_CXX_COMPILER=/usr/local/bin/clang++ && cmake --build .

# FROM busybox:glibc


# COPY --from=0 /usr/local/lib/libc++abi.so.1 /lib
# COPY --from=0 /lib/x86_64-linux-gnu/libpthread.so.0 /lib
# COPY --from=0 /lib/x86_64-linux-gnu/libdl.so.2 /lib
# COPY --from=0 /lib/x86_64-linux-gnu/librt.so.1 /lib
# COPY --from=0 /usr/local/lib/libc++.so.1 /lib
# COPY --from=0 /lib/x86_64-linux-gnu/libm.so.6 /lib
# COPY --from=0 /lib/x86_64-linux-gnu/libgcc_s.so.1 /lib
# COPY --from=0 /lib/x86_64-linux-gnu/libc.so.6 /lib
# COPY --from=0 /lib64/ld-linux-x86-64.so.2 /lib
# COPY --from=0 /usr/lib/x86_64-linux-gnu/libatomic.so.1 /lib
# COPY --from=0 /server/build/bin/project /server/project

CMD [ "/server/project"]


#TODO do not forget to add a way to provide secrets maybe with virtual files