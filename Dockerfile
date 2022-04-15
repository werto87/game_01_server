FROM archlinux:base-devel


COPY . /server

RUN pacman-key --init

RUN pacman -Syu --noconfirm

# use pip here because its recomended way to install conan from the conan team
RUN pacman -S cmake git python-pip --noconfirm

RUN pip install conan

RUN conan profile new default --detect

RUN conan profile update settings.compiler.libcxx=libstdc++11 default

RUN conan remote add gitlab https://gitlab.com/api/v4/projects/27217743/packages/conan

RUN cd /server && rm -rf build ; mkdir build ; cd build ; conan install .. -s build_type=Release --build missing ; cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ; cmake --build . ; cd ..

FROM archlinux:base

COPY --from=0 /server/build/bin/project /server/project

CMD [ "/server/project"]
