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
  %B-t | --target%b                     Specify target - default: %B%F{green}${host_os}-${CPUTYPE}%f%b
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

        cmake_build_args+=(--preset ${_preset} --parallel --config ${config} -- ONLY_ACTIVE_ARCH=NO -arch arm64 -arch x86_64)
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
        local opencv_dir="${project_root}/deps/opencv"
        local opencv_build_dir="${opencv_dir}/build_${target##*-}"

        local -a opencv_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DBUILD_LIST=core,imgproc,objdetect
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DCMAKE_PREFIX_PATH="${advss_dep_path};${_plugin_deps}"
          -DCMAKE_INSTALL_PREFIX="${advss_dep_path}"
        )

        if [ "${target}" != "macos-x86_64" ]; then
          opencv_cmake_args+=(-DWITH_IPP=OFF)
        fi

        pushd ${opencv_dir}
        log_info "Configure OpenCV ..."
        cmake -S . -B ${opencv_build_dir} ${opencv_cmake_args}

        log_info "Building OpenCV ..."
        cmake --build ${opencv_build_dir} --config Release

        log_info "Installing OpenCV..."
        cmake --install ${opencv_build_dir} --prefix "${advss_dep_path}" --config Release || true
        popd

        local leptonica_dir="${project_root}/deps/leptonica"
        local leptonica_build_dir="${leptonica_dir}/build_${target##*-}"

        local -a leptonica_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DSW_BUILD=OFF
          -DOPENJPEG_SUPPORT=OFF
          -DLIBWEBP_SUPPORT=OFF
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

        log_info "Installing Leptonica..."
        # Workaround for "unknown file attribute: H" errors when running install
        cmake --install ${leptonica_build_dir} --prefix "${advss_dep_path}" --config Release || :
        popd

        local tesseract_dir="${project_root}/deps/tesseract"
        local tesseract_build_dir="${tesseract_dir}/build_${target##*-}"

        local -a tesseract_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DSW_BUILD=OFF
          -DBUILD_TRAINING_TOOLS=OFF
          -DCMAKE_PREFIX_PATH="${advss_dep_path};${_plugin_deps}"
          -DCMAKE_INSTALL_PREFIX="${advss_dep_path}"
        )

        if [ "${target}" != "macos-x86_64" ]; then
          tesseract_cmake_args+=(
            -DCMAKE_SYSTEM_PROCESSOR=aarch64
            -DHAVE_AVX=FALSE
            -DHAVE_AVX2=FALSE
            -DHAVE_AVX512F=FALSE
            -DHAVE_FMA=FALSE
            -DHAVE_SSE4_1=FALSE
            -DHAVE_NEON=TRUE
          )
          sed -i'.original' 's/HAVE_NEON FALSE/HAVE_NEON TRUE/g' "${tesseract_dir}/CMakeLists.txt"
        fi

        pushd ${tesseract_dir}
        log_info "Configure Tesseract ..."
        cmake -S . -B ${tesseract_build_dir} ${tesseract_cmake_args}

        log_info "Building Tesseract ..."
        cmake --build ${tesseract_build_dir} --config Release

        log_info "Installing Tesseract..."
        cmake --install ${tesseract_build_dir} --prefix "${advss_dep_path}" --config Release
        popd

        pushd ${advss_dep_path}
        log_info "Prepare openssl ..."
        rm -rf openssl
        git clone https://github.com/openssl/openssl.git --branch openssl-3.2 --depth 1
        mv openssl openssl_x86
        cp -r openssl_x86 openssl_arm

        log_info "Building openssl x86 ..."
        export MACOSX_DEPLOYMENT_TARGET=10.9
        cd openssl_x86
        ./Configure darwin64-x86_64-cc shared
        make

        log_info "Building openssl arm ..."
        export MACOSX_DEPLOYMENT_TARGET=10.15
        cd ../openssl_arm
        ./Configure enable-rc5 zlib darwin64-arm64-cc no-asm
        make

        log_info "Combine arm and x86 openssl binaries ..."
        cd ..
        mkdir openssl-combined
        lipo -create openssl_x86/libcrypto.a openssl_arm/libcrypto.a -output openssl-combined/libcrypto.a
        lipo -create openssl_x86/libssl.a openssl_arm/libssl.a -output openssl-combined/libssl.a

        log_info "Clean up openssl dir..."
        mv openssl_x86 openssl
        rm -rf openssl_arm
        popd

        pushd ${project_root}/deps/libusb
        log_info "Prepare libusb ..."

        log_info "Building libusb x86 (deps) ..."
        export MACOSX_DEPLOYMENT_TARGET=10.9
        mkdir ${project_root}/deps/libusb/out_x86
        ./autogen.sh
        ./configure --host=x86_64-apple-darwin --prefix=${advss_dep_path}
        make && make install

        log_info "Building libusb x86 ..."
        make clean
        rm -r ${project_root}/deps/libusb/out_x86
        mkdir ${project_root}/deps/libusb/out_x86
        ./configure --host=aarch64-apple-darwin --prefix=${project_root}/deps/libusb/out_x86
        make && make install

        log_info "Building libusb arm ..."
        make clean
        export SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
        export CC=$(xcrun --sdk macosx --find clang)
        export CXX=$(xcrun --sdk macosx --find clang++)
        export CFLAGS="-arch arm64 -isysroot $SDKROOT"
        export CXXFLAGS="-arch arm64 -isysroot $SDKROOT"
        export LDFLAGS="-arch arm64 -isysroot $SDKROOT"
        export MACOSX_DEPLOYMENT_TARGET=10.15
        mkdir ${project_root}/deps/libusb/out_arm
        ./configure --host=aarch64-apple-darwin --prefix=${project_root}/deps/libusb/out_arm
        make && make install

        log_info "Combine arm and x86 libusb binaries ..."
        lipo -create ${project_root}/deps/libusb/out_x86/lib/libusb-1.0.0.dylib \
          ${project_root}/deps/libusb/out_arm/lib/libusb-1.0.0.dylib \
          -output ${advss_dep_path}/lib/temp-libusb-1.0.0.dylib
        mv ${advss_dep_path}/lib/temp-libusb-1.0.0.dylib ${advss_dep_path}/lib/libusb-1.0.0.dylib
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
      ;;
    linux)
      # Hacky workaround to support libproc2 with Ubuntu 22 build environment
      local lsb_version=$(lsb_release -r | cut -f 2 || true)
      if [[ "${lsb_version}" == '22.04' ]] {
        sudo apt install ${project_root}/build-aux/CI/linux/ubuntu22/libproc2-0_4.0.2-3_amd64.deb
        sudo apt install ${project_root}/build-aux/CI/linux/ubuntu22/libproc2-dev_4.0.2-3_amd64.deb
      }
      ;;
  }
}

build ${@}
