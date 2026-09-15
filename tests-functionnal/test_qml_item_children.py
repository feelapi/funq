# -*- coding: utf-8 -*-

from base import QmlAppTestCase


class TestQmlItemChildren(QmlAppTestCase):

    def get_children_by_property(self, property_name, recursive=False):
        widget = self.funq.active_widget()
        item = widget.item(id='main')
        children = item.children(recursive=recursive)
        return [child.properties().get(property_name)
                for child in children.iter()
                if property_name in child.properties()]

    def test_non_recursive_children(self):
        children = self.get_children_by_property('text')
        self.assertIn('Parent', children)
        self.assertNotIn('Child', children)

    def test_recursive_children(self):
        children = self.get_children_by_property('text', recursive=True)
        self.assertIn('Parent', children)
        self.assertIn('Child', children)
        self.assertIn('Grandchild', children)
