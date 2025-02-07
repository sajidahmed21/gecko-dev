# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../coreconf/config.gypi',
  ],
  'target_defaults': {
    'variables': {
      'debug_optimization_level': '2',
    },
    'target_conditions': [
      [ '_type=="executable"', {
        'libraries!': [
          '<@(nspr_libs)',
        ],
        'libraries': [
          '<(nss_dist_obj_dir)/lib/libplds4.a',
          '<(nss_dist_obj_dir)/lib/libnspr4.a',
          '<(nss_dist_obj_dir)/lib/libplc4.a',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'fuzz_base',
      'dependencies': [
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/util/util.gyp:nssutil',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7',
        # This is a static build of pk11wrap, softoken, and freebl.
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
      ],
      'conditions': [
        ['fuzz_oss==0', {
          'type': 'static_library',
          'sources': [
            '<!@(ls <(DEPTH)/fuzz/libFuzzer/*.cpp)',
          ],
          'cflags/': [
            ['exclude', '-fsanitize-coverage'],
          ],
          'xcode_settings': {
            'OTHER_CFLAGS/': [
              ['exclude', '-fsanitize-coverage'],
            ],
          },
        }, {
          'type': 'none',
          'all_dependent_settings': {
            'libraries': ['-lFuzzingEngine'],
          }
        }]
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-base',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/lib/freebl/mpi',
        ],
        'sources': [
          'mpi_helper.cc',
        ],
        'conditions': [
          [ 'fuzz_oss==1', {
            'libraries': [
              '/usr/lib/x86_64-linux-gnu/libcrypto.a',
            ],
          }, {
            'libraries': [
              '-lcrypto',
            ],
          }],
        ],
      },
    },
    {
      'target_name': 'nssfuzz-pkcs8',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'pkcs8_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-quickder',
      'type': 'executable',
      'sources': [
        'asn1_mutators.cc',
        'quickder_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-hash',
      'type': 'executable',
      'sources': [
        'hash_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-certDN',
      'type': 'executable',
      'sources': [
        'certDN_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-add',
      'type': 'executable',
      'sources': [
        'mpi_add_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-sub',
      'type': 'executable',
      'sources': [
        'mpi_sub_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-sqr',
      'type': 'executable',
      'sources': [
        'mpi_sqr_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-div',
      'type': 'executable',
      'sources': [
        'mpi_div_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-mod',
      'type': 'executable',
      'sources': [
        'mpi_mod_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-sqrmod',
      'type': 'executable',
      'sources': [
        'mpi_sqrmod_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-addmod',
      'type': 'executable',
      'sources': [
        'mpi_addmod_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-submod',
      'type': 'executable',
      'sources': [
        'mpi_submod_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-mulmod',
      'type': 'executable',
      'sources': [
        'mpi_mulmod_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-mpi-expmod',
      'type': 'executable',
      'sources': [
        'mpi_expmod_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz-mpi-base',
      ],
    },
    {
      'target_name': 'nssfuzz-tls-client',
      'type': 'executable',
      'sources': [
        'tls_client_socket.cc',
        'tls_client_target.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        'fuzz_base',
      ],
      'include_dirs': [
        '<(DEPTH)/lib/freebl',
      ],
    },
    {
      'target_name': 'nssfuzz',
      'type': 'none',
      'dependencies': [
        'nssfuzz-certDN',
        'nssfuzz-hash',
        'nssfuzz-pkcs8',
        'nssfuzz-quickder',
        'nssfuzz-tls-client',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            'nssfuzz-mpi-add',
            'nssfuzz-mpi-addmod',
            'nssfuzz-mpi-div',
            'nssfuzz-mpi-expmod',
            'nssfuzz-mpi-mod',
            'nssfuzz-mpi-mulmod',
            'nssfuzz-mpi-sqr',
            'nssfuzz-mpi-sqrmod',
            'nssfuzz-mpi-sub',
            'nssfuzz-mpi-submod',
          ],
        }],
      ],
    }
  ],
}
