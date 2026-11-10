import unittest
from review_wifi_eapol_types import summarize_header


class EapolTypesTest(unittest.TestCase):
    def test_start_is_not_key(self):
        result=summarize_header(bytes([2,1,0,0]))
        self.assertEqual('Start',result['typeName']);self.assertNotIn('keyDescriptorType',result)

    def test_declared_length_excludes_trailing_bytes(self):
        result=summarize_header(bytes([2,3,0,1,2,0,0x88]))
        self.assertEqual(2,result['keyDescriptorType']);self.assertNotIn('flagCategory',result)
        self.assertEqual(1,result['capturedDeclaredBodyBytes'])
        self.assertEqual(2,result['trailingBytesBeyondDeclaredBody'])

    def test_missing_body_is_not_complete(self):
        result=summarize_header(bytes([2,3,0,95,2,0,0x88]))
        self.assertFalse(result['declaredBodyCaptured'])
        self.assertEqual('pairwise_ack_without_install',result['flagCategory'])

    def test_non_ack_pairwise_is_not_guessed_as_message_two_or_four(self):
        result=summarize_header(bytes([2,3,0,3,2,1,8]))
        self.assertEqual('pairwise_other_or_unresolved',result['flagCategory'])


if __name__=='__main__':unittest.main()
