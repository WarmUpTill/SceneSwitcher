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
  local target="${host_os}-${CPUTYPE}"
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
    macos-x86_64
    macos-arm64
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
        BUILD_CONFIG=${2}
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

  typeset -g QT_VERSION
  typeset -g DEPLOYMENT_TARGET
  typeset -g OBS_DEPS_VERSION
  setup_${host_os}

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

  if [[ -z "${OUT_DIR}" ]] {
    OUT_DIR="advss-build-dependencies"
  }
  mkdir -p "${project_root}/../${OUT_DIR}"
  local advss_dep_path="$(realpath ${project_root}/../${OUT_DIR})"
  local _plugin_deps="${project_root:h}/obs-build-dependencies/plugin-deps-${OBS_DEPS_VERSION}-qt${QT_VERSION}-${target##*-}"

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
        cmake -S . -B ${opencv_build_dir} -G ${generator} ${opencv_cmake_args}

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
        cmake -S . -B ${leptonica_build_dir} -G ${generator} ${leptonica_cmake_args}

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
        cmake -S . -B ${tesseract_build_dir} -G ${generator} ${tesseract_cmake_args}

        log_info "Building Tesseract ..."
        cmake --build ${tesseract_build_dir} --config Release

        log_info "Installing Tesseract..."
        cmake --install ${tesseract_build_dir} --prefix "${advss_dep_path}" --config Release
        popd

        pushd ${advss_dep_path}
        log_info "Prepare openssl ..."
        rm -rf openssl
        git clone git://git.openssl.org/openssl.git --branch openssl-3.1.2 --depth 1
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
      ;;
    linux)
      # Nothing to do for now
      ;;
  }
}

build ${@}
