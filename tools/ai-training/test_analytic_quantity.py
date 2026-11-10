import json
import unittest
from fractions import Fraction
from analytic_quantity import duration,normalize,proposal


class AnalyticQuantityTests(unittest.TestCase):
    def test_exact_rational_conversion_including_cancelling_thirds(self):
        for text,value in [('a second',1000),('a quarter-second',250),('1/3 hour',1200000),('1/3 minute',20000),
                           ('1e-5 hours',36),('3000 microseconds',3),('0.015 minutes',900)]:
            self.assertEqual(value,duration(text))
        for text in ['7','-two seconds','1/7 second','2500 microseconds','0 ms','1.5 milliseconds']:
            self.assertIsNone(duration(text))

    def test_source_bound_missing_unit_asks_without_guessing(self):
        raw=proposal(dict(gap='7'))
        value,reason=normalize(json.dumps(raw),'Set the episode gap to 7.')
        self.assertEqual('CLARIFY',value['status']);self.assertEqual('UNIT_OR_PRECISION',reason)
        raw['episodeGapText']='7 seconds'
        with self.assertRaisesRegex(ValueError,'absent'):normalize(json.dumps(raw),'Set the episode gap to 7.')

    def test_partial_numbers_cannot_change_amount_or_sign(self):
        for original,wrong in [('0.5 seconds','5 seconds'),('-2 seconds','2 seconds')]:
            with self.assertRaises(ValueError):normalize(json.dumps(proposal(dict(follow=wrong))),f'Use {original} for EAPOL.')

    def test_sentence_period_is_not_a_partial_numeric_token(self):
        value,_=normalize(json.dumps(proposal(dict(follow='2.5 s'))),'Use 2.5 s.')
        self.assertEqual(2500,value['followOnMs'])

    def test_missing_settings_cannot_consume_unassigned_quantities(self):
        for prompt in ['Set the gap to seven.','Allow 13 seconds for EAPOL.']:
            value,reason=normalize(json.dumps(proposal({})),prompt)
            self.assertEqual('CLARIFY',value['status']);self.assertEqual('UNASSIGNED_QUANTITY',reason)

    def test_episode_count_cannot_be_borrowed_from_a_time(self):
        raw=proposal(dict(follow='2 seconds',minimum='2'))
        with self.assertRaisesRegex(ValueError,'borrowed'):normalize(json.dumps(raw),'Use 2 seconds, otherwise defaults.')
        value,_=normalize(json.dumps(raw),'Use 2 seconds and require 2 episodes.')
        self.assertEqual(2,value['minLinkedEpisodes'])

    def test_rational_reference_across_numerous_integer_and_fractional_values(self):
        for numerator in range(1,31):
            for denominator in range(1,18):
                expected=Fraction(numerator*60000,denominator)
                value=int(expected) if expected.denominator==1 and expected<=3600000 else None
                self.assertEqual(value,duration(f'{numerator}/{denominator} minutes'))


if __name__=='__main__':unittest.main()
