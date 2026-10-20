import unittest
from pydantic import ValidationError
from labbridge_generated.models.current_request import CurrentRequest


class ModelTests(unittest.TestCase):
    def test_unknown_request_fields_are_not_silently_discarded(self):
        with self.assertRaises(ValidationError):
            CurrentRequest(current_a=0.1, typo=7)
