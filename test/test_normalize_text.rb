require File.dirname(__FILE__) + "/helper.rb"

class NormalizeIPListNormalizeTextTest < Test::Unit::TestCase
  def test_normalize_empty_array_is_empty_array
    assert_equal([], NormalizeIPList.normalize_text([]))
  end

  def test_normalize_removes_duplicates
    assert_equal(['192.168.0.1'],
                 NormalizeIPList.normalize_text(['192.168.0.1/32',
                                                 '192.168.0.1',
                                                 '192.168.0.1/32',
                                                 '192.168.0.1']))
  end

  def test_normalize_fails_on_bad_ip
    assert_raise ArgumentError do
      NormalizeIPList.normalize_text(['192.168.'])
    end
  end

  def test_normalize_strips_extra_bits
    assert_equal(['188.165.0.0/16', '255.255.255.0/24'],
                 NormalizeIPList.normalize_text(['255.255.255.255/24', '188.165.42.1/16']))
  end

  def test_normalize_regression_on_single_private_ip
    assert_equal(['10.0.0.1'],
                 NormalizeIPList.normalize_text(['10.0.0.1']))
  end

  def test_normalize_with_various_wellformed_ips_succeeds
    assert_equal(['1.2.0.0/16',
                  '1.2.3.4',
                  '3.3.3.3',
                  '10.0.0.0/8'],
                 NormalizeIPList.normalize_text(['1.2.3.4', '10.0.0.0/8', '3.3.3.3/32', '1.2.0.0/16']))
  end

  def test_normalize_coalesces_networks
    assert_equal(['10.0.0.0/22'],
                 NormalizeIPList.normalize_text(IPAddr.new('10.0.0.0/22').to_range.to_a.map(&:to_s)))
  end

  def test_normalize_does_not_coalesce_into_complete_class_c_with_missing_element
    ips = IPAddr.new('192.168.1.0/24').to_range.to_a.map(&:to_s) - ['192.168.1.42'] + ['192.168.2.0']
    assert_equal(expand(ips), expand(NormalizeIPList.normalize_text(ips)))
  end

  def expand(ips)
    ips.map { |s| IPAddr.new(s).to_range.to_a.map(&:to_s) }.flatten
  end

  def test_normalize_does_not_coalesce_class_c_with_mask_element
    ips = IPAddr.new('192.168.1.0/24').to_range.to_a.map(&:to_s)
    ips[10] = '192.168.1.10/24'
    assert_equal(expand(['192.168.1.0/24'])+(expand(['192.168.1.0/24'])-['192.168.1.10']),
                 expand(NormalizeIPList.normalize_text(ips)))
  end

  def test_normalize_uniqs_adjacent_discovered_class_cs
    ips = IPAddr.new('192.168.1.0/24').to_range.to_a.map(&:to_s)
    ips[10] = '192.168.1.10/24'
    ips << '192.168.1.10'
    ips << '192.168.1.0/24'
    assert_equal(['192.168.1.0/24'], NormalizeIPList.normalize_text(ips))
  end

  def test_normalize_supports_ranges_which_cross_a_dot
    assert_equal(['192.168.1.255', '192.168.2.0', '192.168.2.1'],
                 NormalizeIPList.normalize_text(['192.168.1.255,192.168.2.1']))
  end

  def test_normalize_and_coalesce_large_range
    assert_equal(['10.0.0.0/8'],
                 NormalizeIPList.normalize_text(['10.0.0.0,10.0.255.255', '10.1.0.0,10.255.255.255']))
  end

  def test_normalize_rejects_unsorted_ranges
    assert_raise ArgumentError do
      NormalizeIPList.normalize_text(['192.168.2.0,192.168.1.192'])
    end
  end

  def test_normalize_rejects_invalid_range
    [['192.168.2.0,'], [',192.168.2.0'], ['192.168.0.0/32,192.168.0.0/32']].each do |a|
      assert_raise ArgumentError do
        NormalizeIPList.normalize_text(a)
      end
    end
  end

  def test_normalize_permits_dumb_single_entry_ranges
    assert_equal(['4.0.0.0'], NormalizeIPList.normalize_text(['4.0.0.0,4.0.0.0']))
  end

  # Issue #3: Some sequences of duplicates are not coalesced properly
  def test_normalize_issue_3
    input = <<EOF.split("\n")
65.96.66.64
65.96.66.64
65.96.66.66
65.96.66.66
65.96.66.67
65.96.66.68
65.96.66.69
65.96.66.69
65.96.66.72
65.96.66.71
65.96.66.71
EOF
    assert_equal(['65.96.66.64', '65.96.66.66', '65.96.66.67',
                  '65.96.66.68', '65.96.66.69', '65.96.66.71',
                  '65.96.66.72'],
                 NormalizeIPList.normalize_text(input))
  end
end
