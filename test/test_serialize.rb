require File.dirname(__FILE__) + "/helper.rb"

class NormalizeIPListTest < Test::Unit::TestCase
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
end
