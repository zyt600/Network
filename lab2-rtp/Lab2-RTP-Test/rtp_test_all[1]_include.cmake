if(EXISTS "/home/aaa/Desktop/lab2-rtp-zyt600/Lab2-RTP-Test/rtp_test_all[1]_tests.cmake")
  include("/home/aaa/Desktop/lab2-rtp-zyt600/Lab2-RTP-Test/rtp_test_all[1]_tests.cmake")
else()
  add_test(rtp_test_all_NOT_BUILT rtp_test_all_NOT_BUILT)
endif()
