# parkouss/funq 未合并 PR 详细分析

## 一、概述

本文档针对 https://github.com/parkouss/funq 仓库中**当前仍处于 OPEN 状态（未合并）的 6 个 Pull Request** 逐一做了真实改动分析。所有结论都基于本地抓取的真实 diff、commit message 和文件清单，没有基于记忆或推测的内容。

- **抓取日期**：2026-09-15
- **OPEN PR 总数**：6 个（仓库总共 53 个 PR）
- **数据来源**：GitHub REST API（pulls.json）+ 本地 bare clone + git diff 完整补丁
- **原始数据存放**：funq\gh\

## 二、数据来源说明

| 文件 | 说明 |
|------|------|
| gh\pulls.json | 全部 53 个 PR 的元数据（792 KB），通过 GET /repos/parkouss/funq/pulls?state=all 一次性抓取 |
| gh\parkouss-funq.git\ | parkouss/funq 的 bare clone 仓库，含 refs/pull/N/head（所有 53 个 PR 的 head 引用） |
| gh\pr\pr-N-meta.json | 6 个 OPEN PR 的 metadata（base/head SHA、author、title、body、created_at、updated_at） |
| gh\pr\pr-N-diff | 6 个 OPEN PR 基于 git diff <base>..refs/pull/N/head 的完整 patch（UTF-8） |
| gh\pr\pr-N-stat.txt | 对应的 --stat 输出（每个文件 + 行数） |

**为什么 base 是老 commit**：部分 PR（如 #7、#17）的 base 指向 bf398436（即 release-1.1.5 tag），原因是这些 PR 创建于 2015 年，作者当时 fork 后未与 master 同步；导致 diff 跨度过大，包含 master 后续独立提交带来的改动。已在每个 PR 的"注意"中标注。

## 三、6 个 OPEN PR 总览

| # | 标题 | 作者 | 创建时间 | base SHA | head SHA | 状态 | 文件 | +/- |
|---|------|------|----------|----------|----------|------|------|-----|
| 7 | Recursively iterate throught GItems data during widget creation | floufen | 2015-04-02 | bf398436 | f33f29a0 | OPEN（10 年） | 81 | +1710/-3042 |
| 17 | refactor player.h and player.cpp | parkouss | 2015-05-18 | bf398436 | 1d6ec2c7 | OPEN（10 年） | 45 | +1136/-1642 |
| 27 | Add quick item childen (can be nested) | rafaeldelucena | 2024-01 | f48b9c2b | eb25bd9a | OPEN | 14 | +288/-3 |
| 60 | Python 2/3 compatibility without 2to3 | dbrgn | 2018-12 | 2ee0b094 | df750f93 | OPEN（7 年） | 23 | +107/-96 |
| 79 | Drop python2 support | rafaeldelucena | 2024-04 | 811a8b4f | 11c72acb | OPEN | 33 | +381/-668 |
| 83 | Migrating from nosetests to pytest | rafaeldelucena | 2024-08（draft） | f48b9c2b | a4ffd56c | OPEN（draft） | 19 | +151/-467 |

注：#83 是 draft PR（PR body 顶部带 "Run functional tests / Run client tests / Add plugin for pytest" 的 TODO 勾选框）。

## 四、各 PR 详细分析

---

### PR #7：Recursively iterate throught GItems data during widget creation

- **作者**：floufen
- **创建时间**：2015-04-02
- **最后更新**：2018-12-10
- **状态**：OPEN 至今（10 年未合并）
- **base/head**：master @ bf398436 → master @ f33f29a0
- **commits**：1 个
- **改动**：81 文件，+1710/-3042

**文件分类**：

- CI / 构建基础设施（删除为主）：`.travis.yml` 删 133 行大幅简化、`appveyor.yml` 删 19 行、`ci/install_dependencies.sh` 删 56 行、新增 `client/pylint.rc` 252 行
- 文档大量重写：`README.rst`、`client/doc/*.rst`
- Python 客户端：`client/funq/` 下 9 个模块重写（client.py 改 421 行、models.py 改 569 行、testcase.py 改 158 行、aliases.py、errors.py、screenshoter.py、tools.py、noseplugin.py）
- 测试目录重整：把 `client/funq/tests/` 移到 `client/tests/`（rename 6 个测试文件）
- C++ 服务端 / libFunq：`player.cpp` 改 247 行、`player.h` 改 63 行、`objectpath.cpp` 改 126 行；新增 `dragndropresponse.h`、`ldPreloadInjector.cpp` 改 39 行；删除 `libFunq.pri` 34 行、`protocole/json.cpp` 3 行
- 功能性测试 app 切换：删除 `tests-functionnal/funq-test-app/`（含 `widgets.h` 251 行、`main.cpp` 69 行、`funq-test-app.pro` 13 行）；新增 `tests-functionnal/app_test.py` 157 行、`run_app.sh` 3 行
- 测试用例：删除 `tests-functionnal/test_action.py` 53 行、`tests-functionnal/test_injection.py` 61 行；修改 `test_click.py`

**注意**：base 指向 bf398436 = release-1.1.5 tag，导致 diff 中大量行其实是 release-1.1.5 之后的 master 独立演化（如 pylint.rc 是后续新增）。**真正的 PR #7 增量是 commit message "Add a mouse move feature" 对应的内容**——该 commit 既做了递归遍历 GItem，又加入了鼠标移动事件。但因为 head 比 master 还多 1 个 commit 之外的差异全部进入 diff，无法精确分离。

**结论**：历史遗留 PR，10 年未合并 + base 严重过时，**实际已经没有合入价值**，重新 cherry-pick 是更可行的做法。

---

### PR #17：refactor player.h and player.cpp

- **作者**：parkouss（仓库 owner 自身）
- **创建时间**：2015-05-18
- **状态**：OPEN 至今（10 年未合并）
- **base/head**：master @ bf398436 → 1d6ec2c7
- **commits**：1 个（commit 1d6ec2c7 "refactor player.h and player.cpp"）
- **改动**：11 个实质文件 + 45 个文件总差异，+1136/-1642

**核心改动**：把 `server/libFunq/player.cpp`（816 行）拆分成多个文件：

| 文件 | 拆分后行数 | 说明 |
|------|-----------|------|
| player.cpp | 816 → 307 | 主体削减 535 行 |
| player_commands_gitems.cpp | 新增 153 | GItems 相关命令 |
| player_commands_itemmodel.cpp | 新增 303 | Model/View 相关命令 |
| player_commands_quickitems.cpp | 新增 122 | QuickItem 相关命令 |
| player_utils.cpp | 新增 90 | 工具函数 |
| player_utils.h | 新增 122 | 工具头文件 |
| player.h | 精简 70 行 | 仅保留主类声明 |

**附属改动**：

- libFunq.pro 加 5 行包含新 cpp
- server/tests/libFunq/libFunq.pro 加 15 行
- dragndropresponse.cpp/h、shortcutresponse.cpp 各加 1 行（include 调整）
- 删除 server/libFunq/libFunq.pri 34 行
- 删除 server/funq_server/runner_mac.py 46 行、appveyor.yml、ci/install_dependencies.sh
- 修改 tests-functionnal/funq.conf、test_click.py、test_injection.py

**注意**：base 是 bf398436 = release-1.1.5，所以 diff 中包含后续 master 演化（如删除 runner_mac.py 对应 macOS 移出 CI）。**真正的 PR #17 增量就是 1d6ec2c7 这个 commit 的 player.cpp 拆分**。

**结论**：纯粹的代码重构（拆文件），不引入新功能。**这是一个本应很早就合并的清理性 PR**，但作者后来已经把大量功能迁移到了 LibrePCB fork 路径，所以一直没合并。

---

### PR #27：Add quick item childen (can be nested)

- **作者**：rafaeldelucena
- **创建时间**：2024-01
- **状态**：OPEN
- **base/head**：master @ f48b9c2b → eb25bd9a
- **commits**：11 个
- **改动**：14 文件，+288/-3

**11 个 commits**（最早→最新）：

| commit | 说明 |
|--------|------|
| 1691774 | **Add quick item childen (can be nested)** — 核心改动 |
| 9e3ee1a | Check for QT_QUICK_LIB at dump_quick_items |
| 7c6a846 | Remove warnings from unused params |
| 26ff92b | Fix flake8 errors |
| c16fd55 | Add Functionnal test |
| f2e6977 | Changes after format_code.sh |
| c49378e | Update Changelog |
| 8f7a2b8 | Fixing functional tests |
| ac4aa77 | Remove unused |
| cbe445a | Add initial version of cmakelists for qml app |
| 35a38df | Updating cmakelists and github action to run qml app test |
| eb25bd9 | Fix cmakelists |

**核心 commit 1691774 的代码改动**：

- server/libFunq/player.cpp：+41 行，新增 dump_quick_items 的 children 收集逻辑（递归遍历 QuickItem 子项）
- server/libFunq/player.h：+1 行（新增方法声明）
- client/funq/models.py：+46/-4 行，新增 children 相关 API
- client/doc/qml_tutorial.rst、client/doc/user_api/widgets_models.rst：文档更新

**后续 commits 配套工作**：

- c16fd55：新增 tests-functionnal/test_qml_item_children.py（55 行）+ tests-functionnal/qml/Children.qml（20 行）
- cbe445a、8f7a2b8：新增 tests-functionnal/funq-test-qml-app/ 完整目录（CMakeLists.txt、main.cpp、qml/children.qml、resources.qrc、.pro）
- 35a38df、eb25bd9：调整 GitHub Actions workflow 以运行 QML 测试
- 9e3ee1a：CMakeLists 里加 find_package(Qt5Quick) 守卫

**结论**：**funq 实现 QML QuickItem 嵌套遍历的关键 PR**，配套了测试 app、CI、文档。base 跟 master 同步（f48b9c2b 是 LibrePCB fork 当前 HEAD），后续 LibrePCB 已经有自己的 children 实现，**与本 PR 形成功能重叠**。

---

### PR #60：Python 2/3 compatibility without 2to3

- **作者**：dbrgn
- **创建时间**：2018-12
- **状态**：OPEN（7 年未合并）
- **base/head**：master @ 2ee0b094 → df750f93
- **commits**：1 个（commit df750f9 "client: Python 2/3 compatibility without 2to3"）
- **改动**：23 文件，+107/-96

**commit message**：

> client: Python 2/3 compatibility without 2to3
>
> The 2to3 option in setup.py had some undesired side effects. Using `six` as a compatibility library is much cleaner.
>
> The code should now be compatible with both Python 2 and 3. The future import should always be used for new Python files.

**改动模式**：

- client/setup.py：把 2to3 选项换成 six 依赖（-7/+4 行）
- client/funq/*.py（client.py / models.py / testcase.py / aliases.py / errors.py / noseplugin.py / screenshoter.py / tools.py）：用 from __future__ import + six 重写 Py2/3 不兼容语法（print、unicode、super()、dict.iteritems()、basestring、urllib2、StringIO、xrange 等）
- client/funq/tests/*.py（test_client、test_models、test_aliases、test_noseplugin、test_screenshoter、test_tools）：每个文件加 2 行 from __future__ import + import six
- tests-functionnal/*.py（test_action、test_click、test_injection、test_retrieve_widget、test_shortcut）：同样的兼容性调整
- client/doc/tutorial_test_1.py、client/doc/tutorial_test_widgets.py：文档示例同步

**结论**：用 six + from __future__ 同时支持 Py2/Py3 的兼容层，**没有引入新功能，纯兼容性变更**。7 年未合并的原因可能是 PR #79 的方向直接放弃了 Py2，让本 PR 失去存在意义。

---

### PR #79：Drop python2 support

- **作者**：rafaeldelucena
- **创建时间**：2024-04
- **状态**：OPEN
- **base/head**：master @ 811a8b4f → 11c72acb
- **commits**：21 个
- **改动**：33 文件，+381/-668

**21 个 commits**（最早→最新）：

| commit | 说明 |
|--------|------|
| 406a971 | After 2to3 |
| af71248 | Remove python2 remaining code |
| 9742875 | Remove python2 ci |
| 5f8b68e | Applying sed to replace nose stuff to pytest |
| ec2407f | Using pytest |
| afa9980 | Fix client tests |
| f446e0a | Add skeleton for plugin |
| ccd962c | WIP |
| e91b603 | fix client tests |
| eb9d7a7 | Simplifying travis for now |
| fc8a8f7 | Dropping travis |
| f252501 | Add github actions demo |
| df8dde8 | Adding tests for Linux on Github actions |
| c58dbde | Add host linux for qt CI config |
| 736f9a0 | Remove demo |
| 4b68333 | Add qmake tool and reduce number of python versions |
| 70595b6 | Remove invalid tool |
| f17c8ee | Build and test separated |
| 553ec5a | Add empty requirements for now |
| 41400fc | add qmake step |
| 11c72ac | run as sudo |

**改动模式**：

- 核心切换：
  - 新增 client/funq/pytest_plugin.py（90 行）—— pytest 插件骨架
  - 删除 client/funq/noseplugin.py（202 行）—— nose 插件移除
  - 删除 client/funq/tests/test_noseplugin.py（93 行）—— 对应测试移除
- CI 平台切换：
  - 删除 .travis.yml（149 行）
  - 新增 .github/workflows/github-actions.yml（88 行）—— GitHub Actions 替代 Travis
  - 新增 requirements.txt
- 代码兼容层清理：所有 client/funq/*.py（aliases / client / errors / models / screenshoter / testcase）去掉 Py2 时代代码（from __future__ import、six、try/except 兼容 import）；改用 Py3 原生语法
- 测试用例同步：tests-functionnal/*.py（test_action、test_click、test_combobox、test_injection、test_retrieve_widget、test_shortcut、test_tableview、test_widget）从 nose 写法切换到 pytest 写法
- 构建：server/setup.py、client/setup.py 简化（移除 Py2 wheel 配置）
- 文档：README.rst、client/doc/conf.py、doc-dev/conf.py、client/doc/tutorial_test_widgets.py 同步更新

**结论**：**本 PR 是 PR #60 的对立方向**——直接放弃 Py2、迁移到 pytest + GitHub Actions。这是一个现代化的完整重构，与 LibrePCB fork 的演进路径基本一致。**未合并的原因可能是 PR #83 正在更细粒度做同样的事情**。

---

### PR #83：Migrating from nosetests to pytest

- **作者**：rafaeldelucena
- **创建时间**：2024-08-11
- **最后更新**：2024-10-04
- **状态**：OPEN（**draft**）
- **base/head**：master @ f48b9c2b → a4ffd56c
- **commits**：18 个
- **改动**：19 文件，+151/-467

**18 个 commits**（最早→最新）：

| commit | 说明 |
|--------|------|
| 4e408ac | Executing nose2pytest command |
| 19c39be | Moving tests to pytest part 1 |
| 74c1eb7 | Remove references for nosetests |
| e8b1fd0 | Fix flake8 issues |
| 391b1d6 | remove nose in setup.py |
| a601349 | Add pytest as requirement in setup.py |
| 9146187 | Fix tests path |
| 1357989 | Change nosetests to pytest in Github actions |
| 4592fa4 | Add verbosity for steps |
| 9d92795 | Add wheel |
| aac9681 | Running pytest directly instead of setup.py test |
| 9232aa3 | Add pytest in setup.py again |
| 43a470f | Add pip install for all archs |
| 812f1dc | Remove test dependencies from setup.py |
| 99276a6 | Updating setup for installs and tests |
| 0e670ca | Add requirements dev |
| a13a58e | Fix install server command |
| a4ffd56 | Not using requirements on the workflow yet |

**PR body**（draft 勾选框）：

> - [ ] Run functional tests
> - [ ] Run client tests
> - [ ] Add plugin for pytest: old version had a nose plugin

**改动模式**：

- 核心删除：
  - 删除 client/funq/noseplugin.py（202 行）—— 与 PR #79 一致
  - 删除 client/funq/tests/test_noseplugin.py（93 行）
- 依赖切换：
  - 新增 client/requirements-dev.txt（+1 行）
  - client/setup.py：移除 nose 相关 test 命令，添加 pytest
- CI 调整：.github/workflows/main.yml（-51 行）：nosetests → pytest
- 测试代码迁移：client/funq/tests/（test_aliases、test_client、test_models、test_screenshoter、test_tools）和 tests-functionnal/setup.cfg 从 nose 写法切换到 pytest 写法
- 文档同步：README.rst、doc-dev/general.rst、client/doc/{gkit_aliases,launching_tests,qml_tutorial,tutorial}.rst

**与 PR #79 的关系**：本 PR 是 **PR #79 的"半成品"子集**——同样删除了 noseplugin 和 test_noseplugin、迁移到 pytest，但**没有新增 pytest_plugin.py**（PR #79 的关键文件）。未合并原因之一就是 PR body 里写着"Add plugin for pytest"还没做。

**结论**：**draft 状态、与 PR #79 部分重叠**。若要合并，优先合并 PR #79（含 pytest 插件）；PR #83 可以直接关闭。

---

## 五、PR 之间的关系与建议

1. **#60 vs #79**：方向相反。
   - #60 用 six 同时支持 Py2/Py3
   - #79 直接放弃 Py2、迁移 pytest
   - 推荐：**关闭 #60，优先合并 #79**

2. **#79 vs #83**：#83 是 #79 的 draft 子集。
   - #83 缺 pytest_plugin.py，未完成 PR body 中承诺的"Add plugin for pytest"
   - 推荐：**关闭 #83，合并 #79**（更完整）

3. **#7 vs #17**：两个 2015 年的老 PR（base 都是 release-1.1.5），跨度大。
   - #7 的真实改动（递归 GItem + mouse move）已被 #27 类似实现覆盖
   - #17 是纯粹的 player.cpp 重构，对应的拆分思路已经反映在 LibrePCB fork 里
   - 推荐：**两个都关闭**，从 LibrePCB fork 重新 cherry-pick 干净版本

4. **#27**：与 LibrePCB fork 的 children 实现重叠。
   - 推荐：**作者本人**（rafaeldelucena）就是 LibrePCB fork 维护者，需要协调两边的实现差异后再考虑合并

## 六、关键统计

- 6 个 OPEN PR 合计：~213 文件改动，+3,773 行 / -5,723 行
- 4 个 PR（#7、#17、#60、#79）跨越 2015–2024 多年演化
- 仅 #27 和 #79 是"现代化方向"，其余多已失去合入价值