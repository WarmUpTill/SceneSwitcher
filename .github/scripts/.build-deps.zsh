#!/usr/bin/env zsh

builtin emulate -L zsh
setopt EXTENDED_GLOB
setopt PUSHD_SILENT
setopt ERR_EXIT
setopt ERR_RETURN
setopt NO_UNSET
setopt PIPE_FAIL
setopt NO_AUTO_PUSHD
setopt NO_PUSHD_IGNORE_DUPS
setopt FUNCTION_ARGZERO

## Enable for script debugging
# setopt WARN_CREATE_GLOBAL
# setopt WARN_NESTED_VAR
# setopt XTRACE

autoload -Uz is-at-least && if ! is-at-least 5.2; then
  print -u2 -PR "%F{1}${funcstack[1]##*/}:%f Running on Zsh version %B${ZSH_VERSION}%b, but Zsh %B5.2%b is the minimum supported version. Upgrade Zsh to fix this issue."
  exit 1
fi

_trap_error() {
  print -u2 -PR '%F{1}    ✖︎ script execution error%f'
  print -PR -e "
    Callstack:
    ${(j:\n     :)funcfiletrace}
  "
  exit 2
}

build() {
  if (( ! ${+SCRIPT_HOME} )) typeset -g SCRIPT_HOME=${ZSH_ARGZERO:A:h}
  local host_os=${${(s:-:)ZSH_ARGZERO:t:r}[3]}
  local project_root=${SCRIPT_HOME:A:h:h}
  local buildspec_file="${project_root}/buildspec.json"

  trap '_trap_error' ZERR

  fpath=("${SCRIPT_HOME}/utils.zsh" ${fpath})
  autoload -Uz log_info log_error log_output set_loglevel check_${host_os} setup_${host_os} setup_obs setup_ccache

  if [[ ! -r ${buildspec_file} ]] {
    log_error \
      'No buildspec.json found. Please create a build specification for your project.' \
      'A buildspec.json.template file is provided in the repository to get you started.'
    return 2
  }

  typeset -g -a skips=()
  local -i _verbosity=1
  local -r _version='1.0.0'
  local -r -a _valid_targets=(
    macos-universal
    macos-arm64
    macos-x86_64
    linux-x86_64
  )
  local -r -a _valid_configs=(Debug RelWithDebInfo Release MinSizeRel)
  if [[ ${host_os} == 'macos' ]] {
    local -r -a _valid_generators=(Xcode Ninja 'Unix Makefiles')
    local generator="${${CI:+Ninja}:-Xcode}"
  } else {
    local -r -a _valid_generators=(Ninja 'Unix Makefiles')
    local generator='Ninja'
  }
  local -r _usage="
Usage: %B${functrace[1]%:*}%b <option> [<options>]

%BOptions%b:

%F{yellow} Build configuration options%f
 -----------------------------------------------------------------------------
  %B-t | --target%b                     Specify target - default: %B%F{green}macos-universal%f%b on macOS, %B%F{green}linux-${CPUTYPE}%f%b on Linux
  %B-c | --config%b                     Build configuration - default: %B%F{green}RelWithDebInfo%f%b
  %B-o | --out%b                        Output directory - default: %B%F{green}RelWithDebInfo%f%b
  %B--generator%b                       Specify build system to generate - default: %B%F{green}Ninja%f%b
                                    Available generators:
                                      - Ninja
                                      - Unix Makefiles
                                      - Xcode (macOS only)

%F{yellow} Output options%f
 -----------------------------------------------------------------------------
  %B-q | --quiet%b                      Quiet (error output only)
  %B-v | --verbose%b                    Verbose (more detailed output)
  %B--skip-[all|build|deps|unpack]%b    Skip all|building OBS|checking for dependencies|unpacking dependencies
  %B--debug%b                           Debug (very detailed and added output)

%F{yellow} General options%f
 -----------------------------------------------------------------------------
  %B-h | --help%b                       Print this usage help
  %B-V | --version%b                    Print script version information"

  local -a args
  while (( # )) {
    case ${1} {
      -t|--target|-c|--config|--generator)
        if (( # == 1 )) || [[ ${2:0:1} == '-' ]] {
          log_error "Missing value for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        ;;
    }
    case ${1} {
      --)
        shift
        args+=($@)
        break
        ;;
      -t|--target)
        if (( ! ${_valid_targets[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        target=${2}
        shift 2
        ;;
      -c|--config)
        if (( ! ${_valid_configs[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        config=${2}
        shift 2
        ;;
      -o|--out)
        OUT_DIR="${2}"
        shift 2
        ;;
      -q|--quiet) (( _verbosity -= 1 )) || true; shift ;;
      -v|--verbose) (( _verbosity += 1 )); shift ;;
      -h|--help) log_output ${_usage}; exit 0 ;;
      -V|--version) print -Pr "${_version}"; exit 0 ;;
      --debug) _verbosity=3; shift ;;
      --generator)
        if (( ! ${_valid_generators[(Ie)${2}]} )) {
          log_error "Invalid value %B${2}%b for option %B${1}%b"
          log_output ${_usage}
          exit 2
        }
        generator=${2}
        shift 2
        ;;
      --skip-*)
        local _skip="${${(s:-:)1}[-1]}"
        local _check=(all deps unpack build)
        (( ${_check[(Ie)${_skip}]} )) || log_warning "Invalid skip mode %B${_skip}%b supplied"
        typeset -g -a skips=(${skips} ${_skip})
        shift
        ;;
      *) log_error "Unknown option: %B${1}%b"; log_output ${_usage}; exit 2 ;;
    }
  }

  if [[ ${host_os} == macos ]] {
    : "${target:=macos-universal}"
  } else {
    : "${target:="${host_os}-${CPUTYPE}"}"
  }

  set -- ${(@)args}
  set_loglevel ${_verbosity}

  check_${host_os}
  setup_ccache

  local product_name
  local product_version
  local git_tag="$(git describe --tags)"

  read -r product_name product_version <<< \
    "$(jq -r '. | {name, version} | join(" ")' ${buildspec_file})"

  if [[ "${git_tag}" =~ '^([0-9]+\.){0,2}(\*|[0-9]+)$' ]] {
    log_info "Using git tag as version identifier '${git_tag}'"
    product_version="${git_tag}"
  } else {
    log_info "Using buildspec.json version identifier '${product_version}'"
  }

  case ${host_os} {
    macos)
      sed -i '' \
        "s/project(\(.*\) VERSION \(.*\))/project(${product_name} VERSION ${product_version})/" \
        "${project_root}/CMakeLists.txt"
      ;;
    linux)
      sed -i'' \
        "s/project(\(.*\) VERSION \(.*\))/project(${product_name} VERSION ${product_version})/"\
        "${project_root}/CMakeLists.txt"
      ;;
  }

  log_info "Run plugin configure step to download OBS deps ..."
  pushd ${project_root}
  if (( ! (${skips[(Ie)all]} + ${skips[(Ie)build]}) )) {
    log_group "Configuring ${product_name}..."

    local -a cmake_args=()
    local -a cmake_build_args=(--build)
    local -a cmake_install_args=(--install)

    case ${_loglevel} {
      0) cmake_args+=(-Wno_deprecated -Wno-dev --log-level=ERROR) ;;
      1) ;;
      2) cmake_build_args+=(--verbose) ;;
      *) cmake_args+=(--debug-output) ;;
    }

    local -r _preset="${target%%-*}${CI:+-ci}"
    case ${target} {
      macos-*)
        if (( ${+CI} )) typeset -gx NSUnbufferedIO=YES

        cmake_args+=(
          -DENABLE_TWITCH_PLUGIN=OFF
          --preset ${_preset}
        )

        # Build a universal binary by default, or a single architecture
        # (e.g. macos-arm64) if requested
        local -a _xcode_archs=(-arch arm64 -arch x86_64)
        if [[ ${target##*-} == universal ]] {
          cmake_args+=(-UCMAKE_OSX_ARCHITECTURES)
        } else {
          cmake_args+=(-DCMAKE_OSX_ARCHITECTURES=${target##*-})
          _xcode_archs=(-arch ${target##*-})
        }

        if (( codesign )) {
          autoload -Uz read_codesign_team && read_codesign_team

          if [[ -z ${CODESIGN_TEAM} ]] {
            autoload -Uz read_codesign && read_codesign
          }
        }

        cmake_args+=(
          -DCODESIGN_TEAM=${CODESIGN_TEAM:-}
          -DCODESIGN_IDENTITY=${CODESIGN_IDENT:--}
        )

        cmake_build_args+=(--preset ${_preset} --parallel --config ${config} -- ONLY_ACTIVE_ARCH=NO ${_xcode_archs})
        cmake_install_args+=(build_macos --config ${config} --prefix "${project_root}/release/${config}")

        local -a xcbeautify_opts=()
        if (( _loglevel == 0 )) xcbeautify_opts+=(--quiet)
        ;;
      linux-*)
        cmake_args+=(
          --preset ${_preset}-${target##*-}
          -G "${generator}"
          -DQT_VERSION=${QT_VERSION:-6}
          -DCMAKE_BUILD_TYPE=${config}
        )

        local cmake_version
        read -r _ _ cmake_version <<< "$(cmake --version)"

        if [[ ${CPUTYPE} != ${target##*-} ]] {
          if is-at-least 3.21.0 ${cmake_version}; then
            cmake_args+=(--toolchain "${project_root}/cmake/linux/toolchains/${target##*-}-linux-gcc.cmake")
          else
            cmake_args+=(-D"CMAKE_TOOLCHAIN_FILE=${project_root}/cmake/linux/toolchains/${target##*-}-linux-gcc.cmake")
          fi
        }

        cmake_build_args+=(--preset ${_preset}-${target##*-} --config ${config})
        if [[ ${generator} == 'Unix Makefiles' ]] {
          cmake_build_args+=(--parallel $(( $(nproc) + 1 )))
        } else {
          cmake_build_args+=(--parallel)
        }

        cmake_install_args+=(build_${target##*-} --prefix ${project_root}/release/${config})
        ;;
    }

    log_debug "Attempting to configure with CMake arguments: ${cmake_args}"

    cmake ${cmake_args}

  }

  popd
  log_group

  if [[ -z "${OUT_DIR}" ]] {
    OUT_DIR=".deps/advss-build-dependencies"
  }
  mkdir -p "${project_root}/${OUT_DIR}"
  local advss_dep_path="$(realpath ${project_root}/${OUT_DIR})"
  local deps_version=$(jq -r '.dependencies.prebuilt.version' ${buildspec_file})
  local qt_deps_version=$(jq -r '.dependencies.qt6.version' ${buildspec_file})
  local _plugin_deps="${project_root:h}/.deps/obs-deps-${deps_version}-${target##*-};${project_root:h}/.deps/obs-deps-qt6-${qt_deps_version}-${target##*-}"

  case ${host_os} {
    macos)
        # Architectures to build the dependencies for. Universal builds are
        # created by building each architecture separately and merging the
        # results using lipo.
        local -a archs=(${target##*-})
        if [[ ${target##*-} == universal ]] archs=(x86_64 arm64)
        local arch

        local opencv_dir="${project_root}/deps/opencv"
        local opencv_contrib_dir="${project_root}/deps/opencv_contrib"
        local opencv_install

        local -a opencv_cmake_args_common=(
          -DCMAKE_BUILD_TYPE=Release
          -DBUILD_LIST=core,imgproc,objdetect,xobjdetect,dnn
          -DOPENCV_EXTRA_MODULES_PATH="${opencv_contrib_dir}/modules"
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DCMAKE_PREFIX_PATH="${advss_dep_path};${_plugin_deps}"
          -DWITH_KLEIDICV=OFF
        )

        pushd ${opencv_dir}

        for arch (${archs}) {
          opencv_install="${opencv_dir}/install_${arch}"

          local -a opencv_cmake_args_arch=(
            -DCMAKE_OSX_ARCHITECTURES=${arch}
            -DCMAKE_INSTALL_PREFIX="${opencv_install}"
          )
          if [[ ${arch} == x86_64 ]] {
            opencv_cmake_args_arch+=(
              -DCMAKE_PROJECT_INCLUDE="${SCRIPT_HOME}/opencv-force-processor-x86_64.cmake"
              -DWITH_IPP=OFF
            )
          }

          log_info "Configure OpenCV (${arch}) ..."
          cmake -S . -B build_${arch} ${opencv_cmake_args_common} ${opencv_cmake_args_arch}

          log_info "Building OpenCV (${arch}) ..."
          cmake --build build_${arch} --config Release

          log_info "Installing OpenCV (${arch}) ..."
          cmake --install build_${arch} --prefix "${opencv_install}" --config Release || true
        }

        # The install of the last architecture provides headers, CMake config
        # files and (for single architecture builds) the libraries
        opencv_install="${opencv_dir}/install_${archs[-1]}"
        cp -R "${opencv_install}/." "${advss_dep_path}"

        if (( ${#archs} > 1 )) {
          log_info "Merging OpenCV into universal binaries ..."
          local opencv_lib
          local opencv_lib_rel
          for opencv_lib in ${opencv_install}/lib/**/*.(dylib|a)(.); do
            opencv_lib_rel="${opencv_lib#${opencv_install}/}"
            local -a opencv_lib_slices=()
            for arch (${archs}) {
              if [[ -f "${opencv_dir}/install_${arch}/${opencv_lib_rel}" ]] {
                opencv_lib_slices+=("${opencv_dir}/install_${arch}/${opencv_lib_rel}")
              }
            }
            if (( ${#opencv_lib_slices} > 1 )) {
              lipo -create ${opencv_lib_slices} -output "${advss_dep_path}/${opencv_lib_rel}"
            }
          done
        }

        for arch (${archs}) {
          rm -rf "${opencv_dir}/install_${arch}"
        }
        popd

        local leptonica_dir="${project_root}/deps/leptonica"
        local leptonica_build_dir="${leptonica_dir}/build_${target##*-}"

        local -a leptonica_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DSW_BUILD=OFF
          -DOPENJPEG_SUPPORT=OFF
          -DENABLE_WEBP=OFF
          -DCMAKE_DISABLE_FIND_PACKAGE_GIF=TRUE
          -DCMAKE_DISABLE_FIND_PACKAGE_JPEG=TRUE
          -DCMAKE_DISABLE_FIND_PACKAGE_TIFF=TRUE
          -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE
          -DCMAKE_PREFIX_PATH="${advss_dep_path};${_plugin_deps}"
          -DCMAKE_INSTALL_PREFIX="${advss_dep_path}"
        )

        pushd ${leptonica_dir}
        log_info "Configure Leptonica ..."
        cmake -S . -B ${leptonica_build_dir} ${leptonica_cmake_args}

        log_info "Building Leptonica ..."
        cmake --build ${leptonica_build_dir} --config Release

        log_info "Installing Leptonica ..."
        # Workaround for "unknown file attribute: H" errors when running install
        cmake --install ${leptonica_build_dir} --prefix "${advss_dep_path}" --config Release || :
        popd

        local tesseract_dir="${project_root}/deps/tesseract"
        local tesseract_build_dir

        pushd ${tesseract_dir}

        for arch (${archs}) {
          # Discard changes made to the sources by the previous architecture's
          # build
          if [[ ${arch} != ${archs[1]} ]] git checkout .

          tesseract_build_dir="${tesseract_dir}/build_${arch}"

          local -a tesseract_cmake_args=(
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_SYSTEM_NAME="Darwin"
            -DCMAKE_OSX_ARCHITECTURES=${arch}
            -DCMAKE_SYSTEM_PROCESSOR=${arch}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
            -DSW_BUILD=OFF
            -DBUILD_TRAINING_TOOLS=OFF
            -DCMAKE_PREFIX_PATH="${advss_dep_path};${_plugin_deps}"
            -DCMAKE_INSTALL_PREFIX="${advss_dep_path}"
          )

          log_info "Configure Tesseract (${arch}) ..."
          cmake -S . -B ${tesseract_build_dir} ${tesseract_cmake_args}

          log_info "Building Tesseract (${arch}) ..."
          cmake --build ${tesseract_build_dir} --config Release
        }

        if (( ${#archs} > 1 )) {
          log_info "Combine libtesseract binaries (${archs}) ..."
          local -a tesseract_libs=()
          for arch (${archs}) {
            mv ${tesseract_dir}/build_${arch}/libtesseract.a ${tesseract_dir}/build_${arch}/libtesseract_${arch}.a
            tesseract_libs+=(${tesseract_dir}/build_${arch}/libtesseract_${arch}.a)
          }
          # The build directory of the last architecture is used for installing
          lipo -create ${tesseract_libs} -output ${tesseract_build_dir}/libtesseract.a
        }

        log_info "Installing Tesseract..."
        cmake --install ${tesseract_build_dir} --prefix "${advss_dep_path}" --config Release

        popd

        log_info "Prepare openssl ..."
        rm -rf ${advss_dep_path}/openssl ${advss_dep_path}/openssl_build
        mkdir ${advss_dep_path}/openssl_build
        pushd ${advss_dep_path}/openssl_build

        rm -rf openssl
        git clone https://github.com/openssl/openssl.git --branch openssl-3.1.2 --depth 1

        for arch (${archs}) {
          cp -r openssl openssl_${arch}
          pushd openssl_${arch}

          log_info "Building openssl (${arch}) ..."
          if [[ ${arch} == x86_64 ]] {
            export MACOSX_DEPLOYMENT_TARGET=10.9
            ./Configure darwin64-x86_64-cc no-shared no-module no-zlib --prefix=${advss_dep_path}
          } else {
            export MACOSX_DEPLOYMENT_TARGET=10.15
            ./Configure enable-rc5 darwin64-arm64-cc no-shared no-module no-asm no-zlib --prefix=${advss_dep_path}
          }
          make -j$(nproc)

          # The last architecture provides headers and (for single architecture
          # builds) the libraries
          if [[ ${arch} == ${archs[-1]} ]] {
            log_info "Install openssl ..."
            make install
          }
          popd
        }

        if (( ${#archs} > 1 )) {
          log_info "Combine openssl binaries (${archs}) ..."
          local -a openssl_crypto_libs=()
          local -a openssl_ssl_libs=()
          for arch (${archs}) {
            openssl_crypto_libs+=(openssl_${arch}/libcrypto.a)
            openssl_ssl_libs+=(openssl_${arch}/libssl.a)
          }
          lipo -create ${openssl_crypto_libs} -output ${advss_dep_path}/lib/libcrypto.a
          lipo -create ${openssl_ssl_libs} -output ${advss_dep_path}/lib/libssl.a
        }

        log_info "Clean up openssl dir ..."
        rm -rf openssl
        for arch (${archs}) {
          rm -rf openssl_${arch}
        }
        popd

        pushd ${project_root}/deps/libusb

        export SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
        export CC=$(xcrun --sdk macosx --find clang)
        export CXX=$(xcrun --sdk macosx --find clang++)
        export MACOSX_DEPLOYMENT_TARGET=10.15

        local libusb_prefix
        local -a libusb_dylibs=()

        for arch (${archs}) {
          log_info "Configure libusb (${arch}) ..."
          export CFLAGS="-arch ${arch} -isysroot $SDKROOT"
          export CXXFLAGS="-arch ${arch} -isysroot $SDKROOT"
          export LDFLAGS="-arch ${arch} -isysroot $SDKROOT"

          # The first architecture is installed into the dependency directory
          # (including headers), any further architecture is installed to a
          # separate location and merged into the installed library afterwards
          if [[ ${arch} == ${archs[1]} ]] {
            libusb_prefix="${advss_dep_path}"
            ./autogen.sh
          } else {
            libusb_prefix="${project_root}/deps/libusb/out_${arch}"
            rm -rf ${libusb_prefix}
            make clean
          }

          log_info "Building libusb (${arch}) ..."
          ./configure --host=${arch/arm64/aarch64}-apple-darwin --prefix=${libusb_prefix}
          make -j$(nproc)
          make install

          libusb_dylibs+=(${libusb_prefix}/lib/libusb-1.0.0.dylib)
        }

        if (( ${#archs} > 1 )) {
          log_info "Combine libusb binaries (${archs}) ..."
          lipo -create ${libusb_dylibs} -output ${advss_dep_path}/lib/temp-libusb-1.0.0.dylib
          mv ${advss_dep_path}/lib/temp-libusb-1.0.0.dylib ${advss_dep_path}/lib/libusb-1.0.0.dylib
        }
        install_name_tool -id @rpath/libusb-1.0.0.dylib ${advss_dep_path}/lib/libusb-1.0.0.dylib
        log_info "Clean up libusb ..."

        unset SDKROOT
        unset CC
        unset CXX
        unset CFLAGS
        unset CXXFLAGS
        unset LDFLAGS
        unset MACOSX_DEPLOYMENT_TARGET

        popd

        local mqtt_dir="${project_root}/deps/paho.mqtt.cpp"
        local mqtt_build_dir="${mqtt_dir}/build_${target##*-}"

        local -a mqtt_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DCMAKE_PREFIX_PATH="${advss_dep_path};${_plugin_deps}"
          -DCMAKE_INSTALL_PREFIX="${advss_dep_path}"
          -DPAHO_BUILD_SHARED=OFF
          -DPAHO_BUILD_STATIC=ON
          -DPAHO_WITH_MQTT_C=ON
          -DPAHO_WITH_SSL=ON
          -DOPENSSL_USE_STATIC_LIBS=ON
        )

        pushd ${mqtt_dir}
        log_info "Configure paho.mqtt.cpp ..."
        cmake -S . -B ${mqtt_build_dir} ${mqtt_cmake_args}

        log_info "Building paho.mqtt.cpp ..."
        cmake --build ${mqtt_build_dir} --config Release

        log_info "Installing paho.mqtt.cpp ..."
        cmake --install ${mqtt_build_dir} --prefix "${advss_dep_path}" --config Release
        popd
      ;;
    linux)
      # Nothing to do for now
      ;;
  }
}

build ${@}
