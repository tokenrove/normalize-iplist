require File.dirname(__FILE__) + "/helper.rb"

class NormalizeIPListValidateTest < Test::Unit::TestCase
  def test_validate_empty_array_is_empty_array
    assert_equal([], NormalizeIPList.validate([]))
  end

  def test_validate_by_default_returns_only_one
    assert_equal([2],
                 NormalizeIPList.validate(['192.168.0.1/32',
                                           '192.168.0.',
                                           '192.168._',
                                           'foo',
                                           '192.168.0.1/32',
                                           '192.168.0.1']))
  end

  def test_validate_valid_range
    assert_equal([],
                 NormalizeIPList.validate(['192.168.0.1,192.168.0.10']))
  end

  def test_validate_range_with_invalid_endpoint
    assert_equal([1],
                 NormalizeIPList.validate(['192.168.0.1,192.168.0.256']))
  end

  def test_validate_with_high_values
    assert_equal([1],
                 NormalizeIPList.validate(['272.208.76.24']))
  end

  def test_validate_range_with_high_values
    assert_equal([1],
                 NormalizeIPList.validate(['255.255.255.255,300.0.0.0']))
  end

  def test_validate_with_supplied_n_returns_up_to_n
    assert_equal([2, 3, 4],
                 NormalizeIPList.validate(['192.168.0.1/32',
                                           '192.168.0.',
                                           '192.168._',
                                           'foo',
                                           '192.168.0.',
                                           'f192.168.0.1'],
                                          3))
  end

  def test_validate_with_various_wellformed_ips_returns_empty_array
    ips = ['1.2.3.4', '10.0.0.0/8', '3.3.3.3/32', '1.2.0.0/16']
    assert_equal([], NormalizeIPList.validate(ips, 3))
  end

  def test_validate_permits_dumb_single_entry_ranges
    assert_equal([], NormalizeIPList.validate(['4.0.0.0,4.0.0.0']))
  end

  def test_validate_stringio
    s = StringIO.new(['192.168.0.1/32',
                      '192.168.0.',
                      '192.168._',
                      'foo',
                      '192.168.0.',
                      'f192.168.0.1'].join("\n"))
    assert_equal([2, 3, 4], NormalizeIPList.validate(s, 3))
  end

  def test_validate_stringio_crlf
    s = StringIO.new(['192.168.0.1/32',
                      '192.168.0.',
                      '192.168._',
                      'foo',
                      '192.168.0.',
                      'f192.168.0.1'].join("\r\n"))
    assert_equal([2, 3, 4], NormalizeIPList.validate(s, 3))
  end

  def test_validate_from_fd
    Tempfile.open('ip-list') do |f|
      f.write(['192.168.0.1/32',
               '192.168.0.',
               '192.00000000000000000000000000000000000000000.1.1',
               '42.0.0.0,0.1.0.0',
               '192.168._',
               'foo',
               '192.168.0.',
               'f192.168.0.1'].join("\n"))
      f.rewind
      assert_equal([2, 5, 6, 7],
                   NormalizeIPList.validate(f, 4))
    end
  end

  def test_validate_from_fd_with_more_entries
    Tempfile.open('ip-list') do |f|
      f.write(['192.168.0.1/32',
               '192.168.0.',
               '192.168._',
               'foo',
               '192.168.0.',
               '192.168.1.42/8',
               '192.168.1.42/0',
               '192.168.1.42/33',
               '192.168.1.42/3.',
               '....',
               '42.42.42.42/.10',
               '255..255.255',
               '255.255.255.255/',
               '255.255.255.255/.',
               '255.255.255./8',
               '255.0255.255.0',
               'f192.168.0.1'].join("\n"))
      f.rewind
      assert_equal([2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17],
                   NormalizeIPList.validate(f, 20))
    end
  end

  def test_validate_zero_padded_ips
    s = StringIO.new(['192.168.000.001/32',
                      '192.168.000.032'].join("\n"))
    assert_equal([], NormalizeIPList.validate(s, 3))
  end

end
