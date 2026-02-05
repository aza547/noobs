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
            ['OS=="linux"', {
                'include_dirs': [
                    "/usr/include/obs"
                ],
                'libraries': [
                    "-lobs",
                    "-lobs-frontend-api",
                    "-lX11"
                ],
                'cflags': [
                    "-fPIC"
                ],
                'cflags_cc': [
                    "-fPIC",
                    "-std=c++17"
                ],
                'defines': [
                    "__linux__"
                ]
            }]
        ]
    }]
}
