{
  "targets": [
    {
      "target_name": "addon",
      "sources": [
        "src/addon.cc",
        "src/db.cc",
        "src/dbenv.cc",
        "src/dbtxn.cc",
        "src/dbcursor.cc",
        "src/flags.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "./deps/db-6.2.23/build_unix"
      ],
      'dependencies': [
        'node_modules/ffi/deps/libffi/libffi.gyp:ffi'
      ],
      "link_settings": {
        "libraries": [
          "-L../lib",
          "-L../deps/db-6.2.23/build_unix",
          "-ldb-6.2"
        ]
      }
    }
  ]
}
