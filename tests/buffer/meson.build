
buffer_tests_list = [
  'blank',
  'buffer_copy',
  'buffer_copy_abyss',
  'buffer_copy_lower_left',
  'buffer_copy_lower_right',
  'buffer_copy_self',
  'buffer_copy_upper_left',
  'buffer_copy_upper_right',
  'buffer_dup_copy1',
  'buffer_dup_copy2',
  'buffer_dup_delete1',
  'buffer_dup_delete2',
  'buffer_dup_write1',
  'buffer_dup_write2',
  'buffer_dup_write3',
  'buffer_dup_write4',
  'buffer_iterator1',
  'buffer_iterator1sub',
  'buffer_iterator2',
  'buffer_iterator2sub',
  'buffer_iterator3',
  'buffer_iterator3sub',
  'buffer_iterator4',
  'buffer_iterator4sub',
  'buffer_linear_copy',
  'buffer_linear_iter',
  'buffer_linear_iter2',
  'buffer_multcopy',
  'buffer_set_pattern',
  'buffer_shift_diagonal',
  'buffer_shift_horizontal',
  'buffer_shift_vertical',
  'checks',
  'disabled_abyss',
  'dup_linear_from_data',
  'get_abyss_black',
  'get_abyss_clamp',
  'get_abyss_empty_buffer',
  'get_abyss_loop',
  'get_abyss_none',
  'get_abyss_white',
  'get_buffer_scaled',
  'get_buffer_scaled2',
  'get_shifted',
  'gray',
  'grow_extent',
  'linear_from_data',
  'linear_from_data_rows',
  'linear_modify',
  'linear_new',
  'linear_proxy_modify',
  'linear_shift',
  'mipmap_iterator',
  'mipmap_iterator2',
  'mipmap_sampler',
  'mipmap_set',
  'mipmap_set2',
  'rect',
  'sample',
  'save_small_roi',
  'sub_rect_fills_and_gets',
  'sub_sub_fill',
  'sub_sub_fill2',
  'vertical_gradient',
]


buffer_test_run_script = find_program('buffer-tests-run.sh')

foreach test_name : buffer_tests_list

  test_exe = executable(test_name,
    'buffer-test-wrapper.c',
    include_directories: [ rootInclude, geglInclude, ],
    link_with: [
      gegl_lib,
    ],
    dependencies: [
      babl,
      glib,
      gobject,
      math,
    ],
    c_args: [
      '-Wno-unused-function',
      '-DTEST_NAME="@0@"'.format(test_name),
      '-DTEST_INCLUDE="@0@"'.format('tests' / test_name + '.c'),
    ],
  )

  test(test_name,
  buffer_test_run_script,
    args: [ test_name, test_exe.full_path(), ],
    env: [
      'GEGL_PATH=' + meson.build_root() / 'operations',
      'GEGL_SWAP=RAM',
      'REFERENCE_DIR=' + meson.source_root() / 'tests' / 'buffer' / 'reference',
    ],
    workdir: meson.current_build_dir(),
    suite: 'buffers',
    is_parallel: false,
  )

endforeach