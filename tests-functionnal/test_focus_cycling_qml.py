from funq.testcase import MultiFunqTestCase


class TestFocusCyclingQml(MultiFunqTestCase):
    __app_config_names__ = ['app_qml_test', 'app_qml_test_2']

    def test_cycle_focus_between_two_qml_apps(self):
        app1 = self.funq['app_qml_test']
        app2 = self.funq['app_qml_test_2']

        for _ in range(10):
            w1 = app1.active_widget()
            self.assertIsNotNone(w1, "app_qml_test should have an active window")

            w2 = app2.active_widget()
            self.assertIsNotNone(w2, "app_qml_test_2 should have an active window")
