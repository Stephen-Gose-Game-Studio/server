#include <platform.h>

#include <kernel/types.h>
#include <kernel/faction.h>
#include <kernel/item.h>
#include <kernel/magic.h>
#include <kernel/region.h>
#include <kernel/spell.h>
#include <kernel/spellbook.h>
#include <kernel/unit.h>
#include <kernel/pool.h>
#include <util/quicklist.h>
#include <util/language.h>

#include <cutest/CuTest.h>
#include <tests.h>

void test_updatespells(CuTest * tc)
{
  faction * f;
  spell * sp;
  spellbook *book = 0;

  test_cleanup();
  
  f = test_create_faction(0);
  sp = create_spell("testspell", 0);
  CuAssertPtrNotNull(tc, sp);

  book = create_spellbook("spells");
  CuAssertPtrNotNull(tc, book);
  spellbook_add(book, sp, 1);

  CuAssertPtrEquals(tc, 0, f->spellbook);
  pick_random_spells(f, 1, book, 1);
  CuAssertPtrNotNull(tc, f->spellbook);
  CuAssertIntEquals(tc, 1, ql_length(f->spellbook->spells));
  CuAssertPtrNotNull(tc, spellbook_get(f->spellbook, sp));
}

void test_spellbooks(CuTest * tc)
{
  spell *sp;
  spellbook *herp, *derp;
  spellbook_entry *entry;
  const char * sname = "herpderp";
  test_cleanup();

  herp = get_spellbook("herp");
  derp = get_spellbook("derp");
  CuAssertPtrNotNull(tc, herp);
  CuAssertPtrNotNull(tc, derp);
  CuAssertTrue(tc, derp!=herp);
  CuAssertStrEquals(tc, "herp", herp->name);
  CuAssertStrEquals(tc, "derp", derp->name);

  sp = create_spell(sname, 0);
  spellbook_add(herp, sp, 1);
  CuAssertPtrNotNull(tc, sp);
  entry = spellbook_get(herp, sp);
  CuAssertPtrNotNull(tc, entry);
  CuAssertPtrEquals(tc, sp, entry->sp);
  /* CuAssertPtrEquals(tc, 0, spellbook_get(derp, sname)); */

  test_cleanup();
  herp = get_spellbook("herp");
  CuAssertPtrNotNull(tc, herp);
  /* CuAssertPtrEquals(tc, 0, spellbook_get(herp, sname)); */
}

void test_pay_spell(CuTest * tc)
{
  spell *sp;
  unit * u;
  faction * f;
  region * r;

  test_cleanup();
  test_create_world();
  r = findregion(0, 0);
  f = test_create_faction(0);
  u = test_create_unit(f, r);
  CuAssertPtrNotNull(tc, u);

  sp = create_spell("testspell", 0);
  CuAssertPtrNotNull(tc, sp);

  sp->components = (spell_component *) calloc(4, sizeof(spell_component));
  sp->components[0].amount = 1;
  sp->components[0].type = rt_find("money");
  sp->components[0].cost = SPC_FIX;
  sp->components[1].amount = 1;
  sp->components[1].type = rt_find("aura");
  sp->components[1].cost = SPC_LEVEL;
  sp->components[2].amount = 1;
  sp->components[2].type = rt_find("horse");
  sp->components[2].cost = SPC_LINEAR;

  set_level(u, SK_MAGIC, 5);
  unit_add_spell(u, 0, sp);

  change_resource(u, rt_find("money"), 1); /* fix costs of 1 money */
  change_resource(u, rt_find("aura"), 3); /* leveled costs of 3 money */
  change_resource(u, rt_find("horse"), 3); /* leveled costs of 3 money */

  pay_spell(u, sp, 3, 1);
  CuAssertIntEquals(tc, 0, get_resource(u, rt_find("money")));
  CuAssertIntEquals(tc, 0, get_resource(u, rt_find("aura")));
  CuAssertIntEquals(tc, 0, get_resource(u, rt_find("horse")));
}

CuSuite *get_magic_suite(void)
{
  CuSuite *suite = CuSuiteNew();
  SUITE_ADD_TEST(suite, test_updatespells);
  SUITE_ADD_TEST(suite, test_spellbooks);
  SUITE_ADD_TEST(suite, test_pay_spell);
  return suite;
}
