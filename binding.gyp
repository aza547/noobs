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
                    "../bin/bin/win64/obs.lib",
                ],
            }],
            ['OS=="linux"', {
                'libraries': [
                    "-L<(module_root_dir)/bin/bin/linux",
                    "-lobs",
                ],
                'ldflags': [
                    "-Wl,-rpath,'$$ORIGIN/../bin/bin/linux'",
                ],
            }],
        ],
    }]
}