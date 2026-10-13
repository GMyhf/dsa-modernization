"""Courseware gate regressions; run with python -m unittest discover -s courseware."""
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import verify
from pptx import Presentation
from pptx.dml.color import RGBColor


class GateRegressionTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.stem = verify.build_all.CHAPTERS['01']
        self.pptx = self.root / (self.stem + '.pptx')
        self.meta = dict(title='Test', subtitle='Subtitle', footer='Footer', info='Info')
        self.modules = {'01': SimpleNamespace(META=self.meta, SLIDES=[])}
        verify.deck.build(self.meta, [], str(self.pptx))
        (self.root / 'README.md').write_text(
            f'| 1 | `{self.stem}` | 1 |\n', encoding='utf-8')
        self.patch = patch.object(verify, 'HERE', self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        verify.FAILURES.clear()
        verify.NOTES.clear()

    def test_unchanged_deck_passes(self):
        verify.check_regenerate(['01'], self.modules)
        self.assertEqual(verify.FAILURES, [])

    def test_moved_shape_is_rejected(self):
        prs = Presentation(self.pptx)
        prs.slides[0].shapes[0].left += 914400
        prs.save(self.pptx)
        verify.check_regenerate(['01'], self.modules)
        self.assertTrue(verify.FAILURES, 'Changed layout must require regeneration')

    def test_changed_background_is_rejected(self):
        prs = Presentation(self.pptx)
        fill = prs.slides[0].background.fill
        fill.solid()
        fill.fore_color.rgb = RGBColor(255, 0, 0)
        prs.save(self.pptx)
        verify.check_regenerate(['01'], self.modules)
        self.assertTrue(verify.FAILURES)

    def test_zip_timestamps_do_not_require_regeneration(self):
        contents = verify.package_contents(self.pptx)
        with zipfile.ZipFile(self.pptx, 'w') as package:
            for name, data in contents.items():
                info = zipfile.ZipInfo(name, date_time=(2000, 1, 1, 0, 0, 0))
                package.writestr(info, data)
        verify.check_regenerate(['01'], self.modules)
        self.assertEqual(verify.FAILURES, [])

    def render(self, bbox, returncode=0):
        def run(cmd, **kwargs):
            output = ''
            code = 0
            if '--convert-to' in cmd:
                out = Path(cmd[cmd.index('--outdir') + 1])
                (out / (self.stem + '.pdf')).touch()
            elif cmd[0] == 'pdffonts':
                output = 'name type encoding emb sub uni object ID\n'
                output += 'ABC+NotoSansCJKsc Type 1 Identity-H yes yes yes 1 0\n'
            elif '-bbox' in cmd:
                output, code = bbox, returncode
            return SimpleNamespace(stdout=output, stderr='probe error', returncode=code)
        with patch.object(verify.subprocess, 'run', side_effect=run):
            verify.check_render(['01'])

    def test_text_extraction_failure_is_rejected(self):
        self.render('', returncode=1)
        self.assertTrue(verify.FAILURES, 'Failed extraction must not pass as zero pages')

    def test_empty_extraction_is_rejected(self):
        self.render('')
        self.assertTrue(verify.FAILURES)

    def test_extra_page_is_rejected(self):
        self.render('<page width="960" height="540"></page>' * 2)
        self.assertTrue(verify.FAILURES)

    def test_valid_render_passes(self):
        self.render('<page width="960" height="540">'
                    '<word xMin="20" yMin="20" xMax="40" yMax="40">Test</word>'
                    '</page>')
        self.assertEqual(verify.FAILURES, [])


class VideoPendingTests(unittest.TestCase):
    """D-043：课件可以先改、视频晚点再合成——但登记表本身也要经得起查。"""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.stem = verify.build_all.CHAPTERS['01']
        self.pptx = self.root / (self.stem + '.pptx')
        meta = dict(title='Test', subtitle='Subtitle', footer='Footer', info='Info')
        self.modules = {'01': SimpleNamespace(META=meta, SLIDES=[])}   # 只有封面 1 页
        verify.deck.build(meta, [], str(self.pptx))
        (self.root / 'content').mkdir()
        (self.root / 'video').mkdir()
        self.script = self.root / 'content' / 'ch01_narration.md'
        self.script.write_text('# 讲稿\n\n---\n\n## P1｜封面\n\n各位同学好。\n', encoding='utf-8')
        self.timeline = self.root / 'video' / 'ch01.timeline.json'
        verify.make_video.write_timeline(
            [dict(page=1, title='封面', clip=3.0, chars=6)], self.timeline,
            verify.make_video.pptx_digest(self.pptx), verify.make_video.script_digest(self.script), 3.0)
        (self.root / 'video' / 'ch01.srt').write_text(
            '1\n00:00:00,000 --> 00:00:03,000\n各位同学好。\n', encoding='utf-8')
        self.pending = self.root / 'video' / 'pending.json'
        self.patch = patch.object(verify, 'HERE', self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        verify.FAILURES.clear()
        verify.NOTES.clear()
        verify.PENDING_VIDEO.clear()

    def register(self, **overrides):
        item = dict(chapter='01', reason='先改课件，攒几处再合成', by='Claude', date='2026-10-13')
        item.update(overrides)
        self.pending.write_text(json.dumps({'pending': [item]}, ensure_ascii=False), encoding='utf-8')

    def make_stale(self):
        self.script.write_text('# 讲稿\n\n---\n\n## P1｜封面\n\n各位同学好，改了一句。\n', encoding='utf-8')

    def test_up_to_date_video_passes(self):
        verify.check_video(['01'], self.modules)
        self.assertEqual(verify.FAILURES, [])

    def test_stale_video_fails_without_registration(self):
        self.make_stale()
        verify.check_video(['01'], self.modules)
        self.assertTrue(any('视频过期' in f for f in verify.FAILURES))

    def test_registered_stale_video_is_a_loud_note(self):
        """变异：登记了也照样 fail → 这里会红。"""
        self.make_stale()
        self.register()
        verify.check_video(['01'], self.modules)
        self.assertEqual(verify.FAILURES, [])
        self.assertEqual(verify.PENDING_VIDEO, ['01'])
        self.assertTrue(any(n.startswith('· ⚠ 视频待合成') and '视频过期' in n for n in verify.NOTES))

    def test_registered_but_up_to_date_fails(self):
        """登记表不许过期。变异：去掉「已同版就判红」→ 这里会红。"""
        self.register()
        verify.check_video(['01'], self.modules)
        self.assertTrue(any('登记该删掉' in f for f in verify.FAILURES))

    def test_narration_mismatch_still_fails_when_registered(self):
        """讲稿是视频的源，不降级。"""
        self.script.write_text('# 讲稿\n\n## P1｜封面\n\n甲。\n\n## P2｜多出来的一页\n\n乙。\n', encoding='utf-8')
        self.register()
        verify.check_video(['01'], self.modules)
        self.assertTrue(any('一页一节' in f for f in verify.FAILURES))

    def test_registration_without_reason_fails(self):
        self.make_stale()
        self.register(reason='')
        verify.check_video(['01'], self.modules)
        self.assertTrue(any('缺 reason' in f for f in verify.FAILURES))
        self.assertTrue(any('视频过期' in f for f in verify.FAILURES), '缺字段的登记不能生效')

    def test_registration_of_unwritten_chapter_fails(self):
        self.register(chapter='02')
        verify.check_video(['01'], self.modules)
        self.assertTrue(any('第02章' in f for f in verify.FAILURES))

    def test_clear_pending_removes_only_that_chapter(self):
        """make_video.py 合成成功后自动删登记。"""
        items = [dict(chapter='01', reason='a', by='Claude', date='2026-10-13'),
                 dict(chapter='02', reason='b', by='Claude', date='2026-10-13')]
        self.pending.write_text(json.dumps({'pending': items}), encoding='utf-8')
        removed = verify.make_video.clear_pending('01', self.pending)
        self.assertEqual(removed['reason'], 'a')
        left = json.loads(self.pending.read_text(encoding='utf-8'))['pending']
        self.assertEqual([it['chapter'] for it in left], ['02'])
        self.assertIsNone(verify.make_video.clear_pending('01', self.pending))


if __name__ == '__main__':
    unittest.main()
