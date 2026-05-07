{
    "targets": [{
        "target_name": "noobs",
        "cflags!": [ "-fno-exceptions" ],
        "cflags_cc!": [ "-fno-exceptions" ],
        "sources": [
            "src/main.cpp",
            "src/obs_interface.cpp",
            "src/utils.cpp",
        ],
        'include_dirs': [
            "<!@(node -p \"require('node-addon-api').include\")",
            "include"
        ],
        'dependencies': [
            "<!(node -p \"require('node-addon-api').gyp\")"
        ],
        'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
        'conditions': [
            ['OS=="win"', {
                'libraries': [
                    "../bin/64bit/obs.lib",
                ],
            }],
            ['OS=="mac"', {
                'sources': [
                    # ObjC++ implementation of the preview NSView path.
                    # The C++ in obs_interface.cpp guards its preview
                    # functions behind _WIN32 so the Mac translation
                    # unit owns those symbols.
                    'src/obs_interface_mac.mm',
                ],
                'libraries': [
                    # Vendored libobs (Phase 5) lives at
                    # noobs/Frameworks/libobs.framework at build time
                    # and at dist/Frameworks/libobs.framework after
                    # dist.js. Both rpaths cover the two layouts:
                    #   build/Release/noobs.node → ../../Frameworks
                    #   dist/noobs.node          → Frameworks
                    '-Wl,-rpath,@loader_path/../../Frameworks',
                    '-Wl,-rpath,@loader_path/Frameworks',
                    '-F<(module_root_dir)/Frameworks',
                    '-framework libobs',
                    '-framework Cocoa',
                ],
                'xcode_settings': {
                    'CLANG_CXX_LANGUAGE_STANDARD': 'c++17',
                    'CLANG_CXX_LIBRARY': 'libc++',
                    'MACOSX_DEPLOYMENT_TARGET': '11.0',
                    # The C++ source uses `throw std::runtime_error(...)` in
                    # several error paths. Default node-gyp on macOS sets
                    # GCC_ENABLE_CPP_EXCEPTIONS = NO and adds -fno-exceptions
                    # via Xcode defaults, which would fail to compile. Re-enable
                    # exceptions for our build.
                    'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                    # ObjC++ default ARC for the .mm — easier memory
                    # management around NSView retain/release.
                    'CLANG_ENABLE_OBJC_ARC': 'YES',
                    'OTHER_CPLUSPLUSFLAGS': [
                        '-Wno-deprecated-declarations',
                    ],
                },
            }],
        ],
    }]
}