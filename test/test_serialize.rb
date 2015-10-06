require File.dirname(__FILE__) + "/helper.rb"

class NormalizeIPListSerializeTest < Test::Unit::TestCase
  def test_serialize_with_empty_array_is_empty
    assert_equal('', NormalizeIPList.serialize([]))
  end

  def test_serialize_with_bad_mask_fails
    assert_raise ArgumentError do
      NormalizeIPList.serialize(['192.168.0.1/42'])
    end
  end

  def test_serialize_with_bad_ip_fails
    assert_raise ArgumentError do
      NormalizeIPList.serialize(['192.168.'])
    end
  end

  def test_serialize_with_various_wellformed_ips_succeeds
    assert_equal([1,2,0,0,16,
                  1,2,3,4,32,
                  3,3,3,3,32,
                  10,0,0,0,8].pack('C*').encode(Encoding::ASCII_8BIT),
                 NormalizeIPList.serialize(['1.2.3.4', '10.0.0.0/8', '3.3.3.3/32', '1.2.0.0/16']))
  end

  def test_serialize_explodes_range
    assert_equal([10,0,0,1,32,
                  10,0,0,2,32,
                  10,0,0,3,32].pack('C*').encode(Encoding::ASCII_8BIT),
                 NormalizeIPList.serialize(['10.0.0.1,10.0.0.3']))
  end

  def test_serialize_on_large_range
    out = NormalizeIPList.serialize(['10.0.0.0,10.0.140.0', '127.0.0.1', '42.0.0.0,42.1.0.0'])
    assert_equal(101379, out.size/5)
  end

  def test_serialize_regression_on_multiple_ranges
    assert_raise ArgumentError do
      NormalizeIPList.serialize(['10.0.0.0,10.0.0.1', '10.0.0.1,10.0.0.0'])
    end
  end
end
