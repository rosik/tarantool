#include "func_cache.h"
#include "func.h"
#include "trivia/util.h"
#include "unit.h"

int status = 0;

struct func *
test_func_new(uint32_t id, const char *name)
{
	uint32_t name_len = strlen(name);
	struct func_def *def = xmalloc(offsetof(struct func_def, name[name_len]));
	def->fid = id;
	memcpy(def->name, name, name_len);
	def->name_len = strlen(name);
	struct func *f = xmalloc(sizeof(struct func));
	f->def = def;
	return f;
}

static void
test_func_delete(struct func *f)
{
	free(f->def);
	free(f);
}

/**
 * Test that pin/is_pinned/unpin works fine with one func and one holder.
 */
static void
func_cache_pin_test_one_holder()
{
	header();
	plan(7);

	func_cache_init();
	struct func *f1 = test_func_new(1, "func1");
	enum func_cache_holder_type type;
	struct func_cache_holder h1;

	func_cache_insert(f1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_pin(f1, &h1, 1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1, "ok");
	func_cache_unpin(f1, &h1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_pin(f1, &h1, 1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1, "ok");
	func_cache_unpin(f1, &h1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_delete(f1->def->fid);

	test_func_delete(f1);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Test several holders that pin/unpins one func in FIFO order.
 */
static void
func_cache_pin_test_fifo()
{
	header();
	plan(8);

	func_cache_init();
	struct func *f1 = test_func_new(1, "func1");
	enum func_cache_holder_type type;
	struct func_cache_holder h1,h2;

	func_cache_insert(f1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_pin(f1, &h1, 1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1, "ok");
	func_cache_pin(f1, &h2, 2);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1 || type == 2, "ok");
	func_cache_unpin(f1, &h1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 2, "ok");
	func_cache_unpin(f1, &h2);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_delete(f1->def->fid);

	test_func_delete(f1);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Test several holders that pin/unpins one func in LIFO order.
 */
static void
func_cache_pin_test_lifo()
{
	header();
	plan(8);

	func_cache_init();
	struct func *f1 = test_func_new(1, "func1");
	enum func_cache_holder_type type;
	struct func_cache_holder h1,h2;

	func_cache_insert(f1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_pin(f1, &h1, 1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1, "ok");
	func_cache_pin(f1, &h2, 2);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1 || type == 2, "ok");
	func_cache_unpin(f1, &h2);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1, "ok");
	func_cache_unpin(f1, &h1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_delete(f1->def->fid);

	test_func_delete(f1);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Test several holders with several funcs.
 */
static void
func_cache_pin_test_several()
{
	header();
	plan(18);

	func_cache_init();
	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");
	enum func_cache_holder_type type;
	struct func_cache_holder h1,h2,h3;

	func_cache_insert(f1);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_pin(f1, &h1, 1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1, "ok");

	func_cache_insert(f2);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(!func_cache_is_pinned(f2, &type), "ok");

	func_cache_pin(f1, &h2, 2);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(!func_cache_is_pinned(f2, &type), "ok");

	func_cache_pin(f2, &h3, 3);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 1 || type == 2, "ok");
	ok(func_cache_is_pinned(f2, &type), "ok");
	ok(type == 3, "ok");

	func_cache_unpin(f1, &h1);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(type == 2, "ok");
	ok(func_cache_is_pinned(f2, &type), "ok");
	ok(type == 3, "ok");

	func_cache_unpin(f2, &h3);
	ok(func_cache_is_pinned(f1, &type), "ok");
	ok(!func_cache_is_pinned(f2, &type), "ok");
	func_cache_delete(f2->def->fid);

	func_cache_unpin(f1, &h3);
	ok(!func_cache_is_pinned(f1, &type), "ok");
	func_cache_delete(f1->def->fid);

	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

struct test_subscr {
	size_t callback_count;
	struct func *found_func;
	struct func_cache_subscription base;
	char func_name[32];
	uint32_t func_name_len;
};

static void
test_subscr_create(struct test_subscr *sub, const char *name)
{
	assert(strlen(name) <= sizeof(sub->func_name));
	sub->func_name_len = strlen(name);
	memcpy(sub->func_name, name, sub->func_name_len);
	sub->callback_count = 0;
	sub->found_func = NULL;
}

static void
test_subscr_destroy(struct test_subscr *sub)
{
	memset(sub->func_name, '#', sizeof(sub->func_name));
	sub->func_name_len = 100500;
	memset(&sub->base, '#', sizeof(sub->base));
}

static void
test_callback(struct func_cache_subscription *base_sub, struct func *func)
{
	struct test_subscr *s = container_of(base_sub, struct test_subscr, base);
	test_subscr_destroy(s);
	s->callback_count++;
	s->found_func = func;
}

/**
 * Subscribe once and check that the callback is called properly.
 */
static void
func_cache_subscribe_test_one()
{
	header();
	plan(3);

	func_cache_init();

	struct test_subscr s;
	test_subscr_create(&s, "func1");

	func_cache_subscribe_by_name(s.func_name, s.func_name_len,
				     &s.base, test_callback);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s.callback_count == 0, "ok");
	func_cache_insert(f1);
	ok(s.callback_count == 1, "ok");
	ok(s.found_func == f1, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe several and check that the callbacks are called properly.
 */
static void
func_cache_subscribe_test_several()
{
	header();
	plan(6);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func2");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f1);
	ok(s1.callback_count == 1, "ok");
	ok(s1.found_func == f1, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f2);
	ok(s1.callback_count == 1, "ok");
	ok(s2.callback_count == 1, "ok");
	ok(s2.found_func == f2, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe twice on the same function and check that the callbacks are called
 * properly.
 */
static void
func_cache_subscribe_test_pair()
{
	header();
	plan(6);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func1");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f1);
	ok(s1.callback_count == 1, "ok");
	ok(s1.found_func == f1, "ok");
	ok(s2.callback_count == 1, "ok");
	ok(s2.found_func == f1, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe once, unsubscribe and ensure that no callbacks are called.
 */
static void
func_cache_subscribe_test_one_unsub()
{
	header();
	plan(2);

	func_cache_init();

	struct test_subscr s;
	test_subscr_create(&s, "func1");

	func_cache_subscribe_by_name(s.func_name, s.func_name_len,
				     &s.base, test_callback);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s.callback_count == 0, "ok");

	func_cache_unsubscribe_by_name(s.func_name, s.func_name_len, &s.base);
	test_subscr_destroy(&s);

	func_cache_insert(f1);
	ok(s.callback_count == 0, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe on two functions, unsubscribe one of then and ensure that only
 * proper callbacks are called.
 */
static void
func_cache_subscribe_test_several_unsub()
{
	header();
	plan(5);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func2");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);

	func_cache_unsubscribe_by_name(s1.func_name, s1.func_name_len, &s1.base);
	test_subscr_destroy(&s1);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f1);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f2);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 1, "ok");
	ok(s2.found_func == f2, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe twice on the same function, unsubscribe the first subscription.
 */
static void
func_cache_subscribe_test_pair_unsub1()
{
	header();
	plan(5);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func1");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);
	func_cache_unsubscribe_by_name(s1.func_name, s1.func_name_len,
				       &s1.base);
	test_subscr_destroy(&s1);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f1);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 1, "ok");
	ok(s2.found_func == f1, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe twice on the same function, unsubscribe the second subscription.
 */
static void
func_cache_subscribe_test_pair_unsub2()
{
	header();
	plan(5);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func1");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);
	func_cache_unsubscribe_by_name(s2.func_name, s2.func_name_len,
				       &s2.base);
	test_subscr_destroy(&s2);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f1);
	ok(s1.callback_count == 1, "ok");
	ok(s1.found_func == f1, "ok");
	ok(s2.callback_count == 0, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe twice on the same function, unsubscribe the first and then the
 * second.
 */
static void
func_cache_subscribe_test_pair_unsub_fifo()
{
	header();
	plan(4);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func1");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);
	func_cache_unsubscribe_by_name(s1.func_name, s1.func_name_len,
				       &s1.base);
	test_subscr_destroy(&s1);
	func_cache_unsubscribe_by_name(s2.func_name, s2.func_name_len,
				       &s2.base);
	test_subscr_destroy(&s2);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f1);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

/**
 * Subscribe twice on the same function, unsubscribe the first and then the
 * second.
 */
static void
func_cache_subscribe_test_pair_unsub_lifo()
{
	header();
	plan(4);

	func_cache_init();

	struct test_subscr s1,s2;
	test_subscr_create(&s1, "func1");
	test_subscr_create(&s2, "func1");

	func_cache_subscribe_by_name(s1.func_name, s1.func_name_len,
				     &s1.base, test_callback);
	func_cache_subscribe_by_name(s2.func_name, s2.func_name_len,
				     &s2.base, test_callback);
	func_cache_unsubscribe_by_name(s2.func_name, s2.func_name_len,
				       &s2.base);
	test_subscr_destroy(&s2);
	func_cache_unsubscribe_by_name(s1.func_name, s1.func_name_len,
				       &s1.base);
	test_subscr_destroy(&s1);

	struct func *f1 = test_func_new(1, "func1");
	struct func *f2 = test_func_new(2, "func2");

	func_cache_insert(f2);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");
	func_cache_insert(f1);
	ok(s1.callback_count == 0, "ok");
	ok(s2.callback_count == 0, "ok");

	func_cache_delete(f1->def->fid);
	func_cache_delete(f2->def->fid);
	test_func_delete(f1);
	test_func_delete(f2);
	func_cache_destroy();

	footer();
	status |= check_plan();
}

int
main()
{
	func_cache_pin_test_one_holder();
	func_cache_pin_test_fifo();
	func_cache_pin_test_lifo();
	func_cache_pin_test_several();

	func_cache_subscribe_test_one();
	func_cache_subscribe_test_several();
	func_cache_subscribe_test_pair();
	func_cache_subscribe_test_one_unsub();
	func_cache_subscribe_test_several_unsub();
	func_cache_subscribe_test_pair_unsub1();
	func_cache_subscribe_test_pair_unsub2();
	func_cache_subscribe_test_pair_unsub_fifo();
	func_cache_subscribe_test_pair_unsub_lifo();
	return status;
}
