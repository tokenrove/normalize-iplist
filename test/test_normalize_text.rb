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

  def test_normalize_with_various_wellformed_ips_succeeds
    assert_equal(['1.2.0.0/16',
                  '1.2.3.4',
                  '3.3.3.3',
                  '10.0.0.0/8'],
                 NormalizeIPList.normalize_text(['1.2.3.4', '10.0.0.0/8', '3.3.3.3/32', '1.2.0.0/16']))
  end
end
