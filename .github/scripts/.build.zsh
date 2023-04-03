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
  local host_os=${${(s:-:)ZSH_ARGZERO:t:r}[2]}
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
  %B-s | --codesign%b                   Enable codesigning (macOS only)
  %B-p | --portable%b                   Enable portable mode (Linux only)
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
      -s|--codesign) CODESIGN=1; shift ;;
      -p|--portable) typeset -g PORTABLE=1; shift ;;
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

  case ${host_os} {
    macos)
      sed -i '' \
        "s/project(\(.*\) VERSION \(.*\))/project(${product_name} VERSION ${product_version})/" \
        "${project_root}/CMakeLists.txt"

        local opencv_dir="${project_root}/deps/opencv"
        local opencv_build_dir="${opencv_dir}/build_${target##*-}"

        local -a opencv_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DBUILD_LIST=core,imgproc,objdetect
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
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
        cmake --install ${opencv_build_dir} --config Release || true
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
        )

        pushd ${leptonica_dir}
        log_info "Configure Leptonica ..."
        cmake -S . -B ${leptonica_build_dir} -G ${generator} ${leptonica_cmake_args}

        log_info "Building Leptonica ..."
        cmake --build ${leptonica_build_dir} --config Release

        log_info "Installing Leptonica..."
        # Workaround for "unknown file attribute: H" errors when running install
        cmake --install ${leptonica_build_dir} --config Release || :
        popd

        local tesseract_dir="${project_root}/deps/tesseract"
        local tesseract_build_dir="${tesseract_dir}/build_${target##*-}"

        local -a tesseract_cmake_args=(
          -DCMAKE_BUILD_TYPE=Release
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DSW_BUILD=OFF
          -DBUILD_TRAINING_TOOLS=OFF
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
        #cmake --build ${tesseract_build_dir} --config Release --target libtesseract
        cmake --build ${tesseract_build_dir} --config Release

        log_info "Installing Tesseract..."
        #cmake --install ${tesseract_build_dir} --config Release --component libtesseract
        cmake --install ${tesseract_build_dir} --config Release
        popd
      ;;
    linux)
      sed -i'' \
        "s/project(\(.*\) VERSION \(.*\))/project(${product_name} VERSION ${product_version})/"\
        "${project_root}/CMakeLists.txt"
      ;;
  }

  setup_obs

  pushd ${project_root}
  if (( ! (${skips[(Ie)all]} + ${skips[(Ie)build]}) )) {
    log_info "Configuring ${product_name}..."

    local _plugin_deps="${project_root:h}/obs-build-dependencies/plugin-deps-${OBS_DEPS_VERSION}-qt${QT_VERSION}-${target##*-}"
    local -a cmake_args=(
      -DCMAKE_BUILD_TYPE=${BUILD_CONFIG:-RelWithDebInfo}
      -DQT_VERSION=${QT_VERSION}
      -DCMAKE_PREFIX_PATH="${_plugin_deps}"
    )

    if (( _loglevel == 0 )) cmake_args+=(-Wno_deprecated -Wno-dev --log-level=ERROR)
    if (( _loglevel  > 2 )) cmake_args+=(--debug-output)

    local num_procs

    case ${target} {
      macos-*)
        autoload -Uz read_codesign
        if (( ${+CODESIGN} )) {
          read_codesign
        }

        num_procs=$(( $(sysctl -n hw.ncpu) + 1 ))

        cmake_args+=(
          -DCMAKE_FRAMEWORK_PATH="${_plugin_deps}/Frameworks"
          -DCMAKE_OSX_ARCHITECTURES=${${target##*-}//universal/x86_64;arm64}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET:-10.15}
          -DOBS_CODESIGN_LINKER=ON
          -DOBS_BUNDLE_CODESIGN_IDENTITY="${CODESIGN_IDENT:--}"
        )

        ;;
      linux-*)
        if (( ${+PORTABLE} )) {
          cmake_args+=(
            -DLINUX_PORTABLE=ON
          )
        } else {
          cmake_args+=(
            -DCMAKE_INSTALL_PREFIX=/usr
            -DLINUX_PORTABLE=OFF
          )
        }
        num_procs=$(( $(nproc) + 1 ))
        ;;
    }

    log_debug "Attempting to configure ${product_name} with CMake arguments: ${cmake_args}"
    cmake -S . -B build_${target##*-} -G ${generator} ${cmake_args}

    log_info "Building ${product_name}..."
    local -a cmake_args=()
    if (( _loglevel > 1 )) cmake_args+=(--verbose)
    if [[ ${generator} == 'Unix Makefiles' ]] cmake_args+=(--parallel ${num_procs})
    cmake --build build_${target##*-} --config ${BUILD_CONFIG:-RelWithDebInfo} ${cmake_args}
  }

  log_info "Installing ${product_name}..."
  local -a cmake_args=()
  if (( _loglevel > 1 )) cmake_args+=(--verbose)
  cmake --install build_${target##*-} --config ${BUILD_CONFIG:-RelWithDebInfo} --prefix "${project_root}/release" ${cmake_args}
  popd
}

build ${@}
