"""
tests/save_fixtures/test_save_fixtures.py — Structural validation of M26 save fixtures.

TEACHING NOTE — Fixture Validation Tests
==========================================
These tests do NOT require building the C++ engine.  They are pure Python
JSON-level checks that run on Linux CI alongside the cook-pipeline and
animation-authoring tests.

Their purpose is threefold:

  1. **Schema guard** — verify every fixture in this directory is valid JSON
     and contains the top-level fields that SaveSystem::Load() requires.

  2. **Migration contract** — verify that each fixture carries a version
     string that is *not* the current production version ("1.0.0"), so the
     C++ loader's migration ladder is actually exercised during save_test.

  3. **Content smoke-test** — spot-check that the fixture's component data is
     reasonable (positive HP, valid quest IDs, etc.) so a broken fixture is
     caught here before it silently yields a wrong assertion in C++ CI.

The C++ acceptance tests (``--headless --scene save_test`` in
``src/sandbox/main.cpp``) load these fixtures via SaveSystem::Load() to
exercise the actual C++ migration path.  See the ``migration`` subtest block
in that file and ``tests/save_fixtures/README.md`` for details.

Run with:
    pytest tests/save_fixtures/ -v
    # or from repo root:
    python -m pytest tests/save_fixtures/ -v
"""

from __future__ import annotations

import json
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

FIXTURES_DIR = Path(__file__).parent
"""Directory containing all save fixture JSON files."""

CURRENT_VERSION = "1.0.0"
"""
Production save-format version embedded by SaveSystem::Save().
Fixtures must carry an OLDER version so the migration path is exercised.
This constant must stay in sync with ``kSaveFormatVersion`` in
``src/engine/save/save_schema.hpp``.
"""

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _load_fixture(name: str) -> dict:
    """Load and parse a fixture JSON file from FIXTURES_DIR.

    Parameters
    ----------
    name:
        Filename (e.g. ``"v0_9_0_player.save.json"``).

    Returns
    -------
    dict
        Parsed fixture data.
    """
    path = FIXTURES_DIR / name
    assert path.exists(), f"Fixture not found: {path}"
    with path.open(encoding="utf-8") as fh:
        return json.load(fh)


def _all_fixture_paths() -> list[Path]:
    """Return all ``*.save.json`` files in FIXTURES_DIR."""
    return sorted(FIXTURES_DIR.glob("*.save.json"))


# ---------------------------------------------------------------------------
# Generic fixture-discovery tests (parametrised over every *.save.json)
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "fixture_path",
    _all_fixture_paths(),
    ids=[p.name for p in _all_fixture_paths()],
)
def test_fixture_is_valid_json(fixture_path: Path) -> None:
    """Every *.save.json in this directory must be parseable JSON.

    TEACHING NOTE — Fail-fast on parse errors
    ------------------------------------------
    If a fixture is accidentally corrupted (e.g. a manual edit leaves a
    trailing comma), this test surfaces the error immediately with a clear
    message rather than letting it propagate into a cryptic C++ load failure.
    """
    with fixture_path.open(encoding="utf-8") as fh:
        data = json.load(fh)  # raises json.JSONDecodeError on bad JSON
    assert isinstance(data, dict), f"{fixture_path.name}: top-level must be a JSON object"


@pytest.mark.parametrize(
    "fixture_path",
    _all_fixture_paths(),
    ids=[p.name for p in _all_fixture_paths()],
)
def test_fixture_has_required_top_level_fields(fixture_path: Path) -> None:
    """Each fixture must have the fields SaveSystem::Load() depends on.

    TEACHING NOTE — Required vs optional fields
    ---------------------------------------------
    SaveSystem::Load() calls ``root.contains("entities")`` before accessing
    the entity array, and returns false (with LOG_ERROR) when it is absent.
    The ``version`` field is read via ``root.value("version", "0.0.0")``, so
    it technically has a default — but every fixture must explicitly set it
    so the migration-contract test below can assert an old version.
    The ``savedAt`` and ``gameTimeSecs`` fields are informational; the loader
    does not crash without them, but they make fixtures human-readable.
    """
    with fixture_path.open(encoding="utf-8") as fh:
        data = json.load(fh)

    for field in ("version", "savedAt", "gameTimeSecs", "locationName", "entities"):
        assert field in data, (
            f"{fixture_path.name}: missing required field '{field}'"
        )

    assert isinstance(data["entities"], list), (
        f"{fixture_path.name}: 'entities' must be a JSON array"
    )


@pytest.mark.parametrize(
    "fixture_path",
    _all_fixture_paths(),
    ids=[p.name for p in _all_fixture_paths()],
)
def test_fixture_version_is_older_than_current(fixture_path: Path) -> None:
    """Every fixture must carry a version string older than the current format.

    TEACHING NOTE — Why fixtures must NOT be at "1.0.0"
    -------------------------------------------------------
    The whole point of a migration fixture is to exercise the migration
    ladder in SaveSystem::Load().  If a fixture's version equals the current
    kSaveFormatVersion, the loader skips the migration branch entirely, and
    the migration subtest of save_test becomes a no-op.

    This test ensures that no one accidentally updates a fixture to the
    current version (e.g. after a format bump) without also bumping
    kSaveFormatVersion and creating a new older-version fixture.
    """
    with fixture_path.open(encoding="utf-8") as fh:
        data = json.load(fh)

    stored = data.get("version", "")
    assert stored != CURRENT_VERSION, (
        f"{fixture_path.name}: version is '{stored}' which equals the current "
        f"production version '{CURRENT_VERSION}'.  Migration fixtures must use "
        f"an older version string so the SaveSystem migration path is exercised."
    )
    assert stored, (
        f"{fixture_path.name}: 'version' field must not be empty"
    )


@pytest.mark.parametrize(
    "fixture_path",
    _all_fixture_paths(),
    ids=[p.name for p in _all_fixture_paths()],
)
def test_fixture_entities_have_components(fixture_path: Path) -> None:
    """Every entity in every fixture must have a non-empty 'components' object.

    TEACHING NOTE — SaveSystem entity format
    ------------------------------------------
    SaveSystem::Load() skips entities that lack a ``"components"`` key (with a
    LOG_WARN).  Fixtures should not rely on that fallback — every entity must
    supply its component map so the loader can actually restore it.
    """
    with fixture_path.open(encoding="utf-8") as fh:
        data = json.load(fh)

    for idx, entity in enumerate(data["entities"]):
        assert "components" in entity, (
            f"{fixture_path.name}: entity[{idx}] missing 'components' key"
        )
        assert isinstance(entity["components"], dict), (
            f"{fixture_path.name}: entity[{idx}]['components'] must be a JSON object"
        )
        assert entity["components"], (
            f"{fixture_path.name}: entity[{idx}]['components'] must not be empty"
        )


# ---------------------------------------------------------------------------
# Specific tests for v0_9_0_player.save.json
# ---------------------------------------------------------------------------

V0_FIXTURE = "v0_9_0_player.save.json"


def test_v0_fixture_exists() -> None:
    """The canonical v0.9.0 migration fixture must exist."""
    assert (FIXTURES_DIR / V0_FIXTURE).exists(), (
        f"M26 migration fixture '{V0_FIXTURE}' not found in {FIXTURES_DIR}. "
        "This fixture is required by the save_test migration subtest."
    )


def test_v0_fixture_version_is_0_9_0() -> None:
    """v0_9_0_player.save.json must report version '0.9.0'.

    TEACHING NOTE — Migration ladder entry point
    ----------------------------------------------
    The C++ migration subtest in save_test writes this exact file to a temp
    slot, then calls SaveSystem::Load().  The loader compares the stored
    version ("0.9.0") to kSaveFormatVersion ("1.0.0"), logs a migration
    warning, and continues.  This test guarantees the fixture carries the
    exact version string the C++ test expects.
    """
    data = _load_fixture(V0_FIXTURE)
    assert data["version"] == "0.9.0", (
        f"Expected version '0.9.0' in {V0_FIXTURE}, got '{data['version']}'"
    )


def test_v0_fixture_has_player_entity() -> None:
    """The v0.9.0 fixture must contain exactly one entity with Name + Health.

    TEACHING NOTE — Representative fixture content
    ------------------------------------------------
    The migration subtest asserts that HP is preserved across the migration.
    This requires the fixture to carry at least one entity with both a
    ``"Name"`` component (to identify the player) and a ``"Health"``
    component (to verify HP is not lost during migration).
    """
    data = _load_fixture(V0_FIXTURE)
    entities = data["entities"]
    assert len(entities) >= 1, f"{V0_FIXTURE}: must have at least one entity"

    player = entities[0]["components"]
    assert "Name" in player, f"{V0_FIXTURE}: player entity must have 'Name' component"
    assert "Health" in player, f"{V0_FIXTURE}: player entity must have 'Health' component"


def test_v0_fixture_player_hp_is_positive() -> None:
    """Player HP in the v0.9.0 fixture must be a positive integer.

    TEACHING NOTE — Smoke-testing fixture values
    ----------------------------------------------
    If hp were 0 or negative the migration subtest assertion "hp unchanged after
    load" would pass vacuously even if the loader silently zeroed all HP fields
    (e.g. due to a missing field name).  Requiring hp > 0 ensures the assertion
    is meaningful.
    """
    data = _load_fixture(V0_FIXTURE)
    health = data["entities"][0]["components"]["Health"]
    hp     = health.get("hp", 0)
    max_hp = health.get("maxHp", 0)
    assert hp > 0,     f"{V0_FIXTURE}: player hp must be > 0 (got {hp})"
    assert max_hp > 0, f"{V0_FIXTURE}: player maxHp must be > 0 (got {max_hp})"
    assert hp <= max_hp, (
        f"{V0_FIXTURE}: hp ({hp}) must not exceed maxHp ({max_hp})"
    )


def test_v0_fixture_player_has_position() -> None:
    """The v0.9.0 player entity must include a Transform with (px, py, pz).

    TEACHING NOTE — Positional data in v0.9.0
    -------------------------------------------
    In version 0.9.0, the Transform component only stored position — rotation
    and scale fields were added in 1.0.0.  The C++ loader fills in defaults
    (rotation=0, scale=1) when those fields are absent, so the fixture
    deliberately omits them to exercise that default-fill code path.
    """
    data = _load_fixture(V0_FIXTURE)
    comps = data["entities"][0]["components"]
    assert "Transform" in comps, (
        f"{V0_FIXTURE}: player entity must have 'Transform' component"
    )
    transform = comps["Transform"]
    for axis in ("px", "py", "pz"):
        assert axis in transform, (
            f"{V0_FIXTURE}: Transform missing position field '{axis}'"
        )
        assert isinstance(transform[axis], (int, float)), (
            f"{V0_FIXTURE}: Transform['{axis}'] must be a number"
        )

    # Verify rotation/scale are absent (they were added in 1.0.0).
    # This is the key differentiator of a v0.9.0 fixture: the loader must
    # apply default values for rx/ry/rz and sx/sy/sz.
    for absent_field in ("rx", "ry", "rz", "sx", "sy", "sz"):
        assert absent_field not in transform, (
            f"{V0_FIXTURE}: v0.9.0 Transform should NOT contain '{absent_field}' "
            f"(those fields were added in v1.0.0; their absence exercises the "
            f"loader's default-value code path)."
        )


def test_v0_fixture_has_quest_subsystem_state() -> None:
    """The v0.9.0 fixture must include Quest component data.

    TEACHING NOTE — Multi-subsystem fixture coverage
    -------------------------------------------------
    The M26 requirements specify that the fixture must cover "at least one
    other subsystem state (quest/inventory)".  Including Quest data verifies
    that the migration path preserves quest progress, not just HP and position.
    This prevents a future refactor from accidentally dropping quest entries
    during a migration.
    """
    data = _load_fixture(V0_FIXTURE)
    comps = data["entities"][0]["components"]
    assert "Quest" in comps, (
        f"{V0_FIXTURE}: player entity must have 'Quest' component "
        "to exercise multi-subsystem migration coverage"
    )
    quests = comps["Quest"]
    assert isinstance(quests, list), f"{V0_FIXTURE}: 'Quest' must be a JSON array"
    assert len(quests) >= 1, f"{V0_FIXTURE}: 'Quest' array must have at least one entry"

    for idx, q in enumerate(quests):
        assert "questID"   in q, f"{V0_FIXTURE}: Quest[{idx}] missing 'questID'"
        assert "progress"  in q, f"{V0_FIXTURE}: Quest[{idx}] missing 'progress'"
        assert "required"  in q, f"{V0_FIXTURE}: Quest[{idx}] missing 'required'"
        assert "isComplete" in q, f"{V0_FIXTURE}: Quest[{idx}] missing 'isComplete'"
        assert isinstance(q["questID"], int), (
            f"{V0_FIXTURE}: Quest[{idx}]['questID'] must be an integer"
        )
        assert q["questID"] > 0, (
            f"{V0_FIXTURE}: Quest[{idx}]['questID'] must be > 0 (got {q['questID']})"
        )


def test_v0_fixture_has_currency_state() -> None:
    """The v0.9.0 fixture must include Currency component data (gil ≥ 0).

    TEACHING NOTE — Verifying economic state preservation
    -------------------------------------------------------
    Gil (in-game currency) is a critical game-state field.  A migration that
    silently zeroes gil would be catastrophic in a real game.  Including
    Currency in the fixture and asserting it round-trips correctly catches
    this class of regression.
    """
    data = _load_fixture(V0_FIXTURE)
    comps = data["entities"][0]["components"]
    assert "Currency" in comps, (
        f"{V0_FIXTURE}: player entity must have 'Currency' component"
    )
    currency = comps["Currency"]
    assert "gil" in currency, f"{V0_FIXTURE}: Currency missing 'gil'"
    assert isinstance(currency["gil"], int), (
        f"{V0_FIXTURE}: Currency['gil'] must be an integer (got {type(currency['gil']).__name__})"
    )
    assert currency["gil"] >= 0, (
        f"{V0_FIXTURE}: Currency['gil'] must be >= 0 (got {currency['gil']})"
    )
