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
                    'src/obs_interface_mac.mm',
                ],
                'libraries': [
                    # Build-tree and dist-tree runtime paths.
                    '-Wl,-rpath,@loader_path/../../Frameworks',
                    '-Wl,-rpath,@loader_path/Frameworks',
                    '-F<(module_root_dir)/Frameworks',
                    '-framework libobs',
                    '-framework Cocoa',
                ],
                'xcode_settings': {
                    'CLANG_CXX_LANGUAGE_STANDARD': 'c++17',
                    'CLANG_CXX_LIBRARY': 'libc++',
                    'MACOSX_DEPLOYMENT_TARGET': '12.0',
                    'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                    'CLANG_ENABLE_OBJC_ARC': 'YES',
                    'OTHER_CPLUSPLUSFLAGS': [
                        '-Wno-deprecated-declarations',
                    ],
                },
            }],
        ],
    }]
}
