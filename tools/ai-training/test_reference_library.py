import unittest
from build_reference_library import passages


class ReferenceLibraryTests(unittest.TestCase):
    def test_chunks_are_bounded_and_do_not_duplicate_words(self):
        text=' '.join('word'+str(i) for i in range(440))
        chunks=passages(text)
        self.assertEqual(2,len(chunks))
        self.assertEqual(text,' '.join(chunks))

    def test_tiny_footer_is_not_a_passage(self):
        self.assertEqual([],passages('page 12'))

    def test_whitespace_normalized(self):
        self.assertEqual(20,len(passages(('a\n\t'*20))[0].split()))


if __name__=='__main__':unittest.main()
