/*
 * Copyright (c) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/kobject.h>
#include <zephyr/sys/libc-hooks.h>
#include <zephyr/app_memory/mem_domain.h>
#include <zephyr/sys/util_loops.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/rtio/rtio_spsc.h>
#include <zephyr/rtio/rtio_mpsc.h>
#include <zephyr/rtio/rtio.h>
#include <zephyr/rtio/rtio_executor_simple.h>
#include <zephyr/rtio/rtio_executor_concurrent.h>

#include "rtio_iodev_test.h"

/*
 * @brief Produce and Consume a single uint32_t in the same execution context
 *
 * @see rtio_spsc_acquire(), rtio_spsc_produce(), rtio_spsc_consume(), rtio_spsc_release()
 *
 * @ingroup rtio_tests
 */
ZTEST(rtio_spsc, test_produce_consume_size1)
{
	RTIO_SPSC_DEFINE(ezspsc, uint32_t, 1);

	const uint32_t magic = 43219876;

	uint32_t *acq = rtio_spsc_acquire(&ezspsc);

	zassert_not_null(acq, "Acquire should succeed");

	*acq = magic;

	uint32_t *acq2 = rtio_spsc_acquire(&ezspsc);

	zassert_is_null(acq2, "Acquire should fail");

	uint32_t *cons = rtio_spsc_consume(&ezspsc);

	zassert_is_null(cons, "Consume should fail");

	zassert_equal(rtio_spsc_consumable(&ezspsc), 0, "Consumables should be 0");

	rtio_spsc_produce(&ezspsc);

	zassert_equal(rtio_spsc_consumable(&ezspsc), 1, "Consumables should be 1");

	uint32_t *cons2 = rtio_spsc_consume(&ezspsc);

	zassert_equal(rtio_spsc_consumable(&ezspsc), 0, "Consumables should be 0");

	zassert_not_null(cons2, "Consume should not fail");
	zassert_equal(*cons2, magic, "Consume value should equal magic");

	uint32_t *cons3 = rtio_spsc_consume(&ezspsc);

	zassert_is_null(cons3, "Consume should fail");


	uint32_t *acq3 = rtio_spsc_acquire(&ezspsc);

	zassert_is_null(acq3, "Acquire should not succeed");

	rtio_spsc_release(&ezspsc);

	uint32_t *acq4 = rtio_spsc_acquire(&ezspsc);

	zassert_not_null(acq4, "Acquire should succeed");
}

/*&*
 * @brief Produce and Consume 3 items at a time in a spsc of size 4 to validate masking
 * and wrap around reads/writes.
 *
 * @see rtio_spsc_acquire(), rtio_spsc_produce(), rtio_spsc_consume(), rtio_spsc_release()
 *
 * @ingroup rtio_tests
 */
ZTEST(rtio_spsc, test_produce_consume_wrap_around)
{
	RTIO_SPSC_DEFINE(ezspsc, uint32_t, 4);

	for (int i = 0; i < 10; i++) {
		zassert_equal(rtio_spsc_consumable(&ezspsc), 0, "Consumables should be 0");
		for (int j = 0; j < 3; j++) {
			uint32_t *entry = rtio_spsc_acquire(&ezspsc);

			zassert_not_null(entry, "Acquire should succeed");
			*entry = i * 3 + j;
			rtio_spsc_produce(&ezspsc);
		}
		zassert_equal(rtio_spsc_consumable(&ezspsc), 3, "Consumables should be 3");

		for (int k = 0; k < 3; k++) {
			uint32_t *entry = rtio_spsc_consume(&ezspsc);

			zassert_not_null(entry, "Consume should succeed");
			zassert_equal(*entry, i * 3 + k, "Consume value should equal i*3+k");
			rtio_spsc_release(&ezspsc);
		}

		zassert_equal(rtio_spsc_consumable(&ezspsc), 0, "Consumables should be 0");

	}
}

/**
 * @brief Ensure that integer wraps continue to work.
 *
 * Done by setting all values to UINTPTR_MAX - 2 and writing and reading enough
 * to ensure integer wraps occur.
 */
ZTEST(rtio_spsc, test_int_wrap_around)
{
	RTIO_SPSC_DEFINE(ezspsc, uint32_t, 4);
	ezspsc._spsc.in = ATOMIC_INIT(UINTPTR_MAX - 2);
	ezspsc._spsc.out = ATOMIC_INIT(UINTPTR_MAX - 2);

	for (int j = 0; j < 3; j++) {
		uint32_t *entry = rtio_spsc_acquire(&ezspsc);

		zassert_not_null(entry, "Acquire should succeed");
		*entry = j;
		rtio_spsc_produce(&ezspsc);
	}

	zassert_equal(atomic_get(&ezspsc._spsc.in), UINTPTR_MAX + 1, "Spsc in should wrap");

	for (int k = 0; k < 3; k++) {
		uint32_t *entry = rtio_spsc_consume(&ezspsc);

		zassert_not_null(entry, "Consume should succeed");
		zassert_equal(*entry, k, "Consume value should equal i*3+k");
		rtio_spsc_release(&ezspsc);
	}

	zassert_equal(atomic_get(&ezspsc._spsc.out), UINTPTR_MAX + 1, "Spsc out should wrap");
}

#define MAX_RETRIES 5
#define SMP_ITERATIONS 100

RTIO_SPSC_DEFINE(spsc, uint32_t, 4);

static void t1_consume(void *p1, void *p2, void *p3)
{
	struct rtio_spsc_spsc *ezspsc = p1;
	uint32_t retries = 0;
	uint32_t *val = NULL;

	for (int i = 0; i < SMP_ITERATIONS; i++) {
		val = NULL;
		retries = 0;
		while (val == NULL && retries < MAX_RETRIES) {
			val = rtio_spsc_consume(ezspsc);
			retries++;
		}
		if (val != NULL) {
			rtio_spsc_release(ezspsc);
		} else {
			printk("consumer yield\n");
			k_yield();
		}
	}
}

static void t2_produce(void *p1, void *p2, void *p3)
{
	struct rtio_spsc_spsc *ezspsc = p1;
	uint32_t retries = 0;
	uint32_t *val = NULL;

	for (int i = 0; i < SMP_ITERATIONS; i++) {
		val = NULL;
		retries = 0;
		printk("producer acquiring\n");
		while (val == NULL && retries < MAX_RETRIES) {
			val = rtio_spsc_acquire(ezspsc);
			retries++;
		}
		if (val != NULL) {
			*val = SMP_ITERATIONS;
			rtio_spsc_produce(ezspsc);
		} else {
			printk("producer yield\n");
			k_yield();
		}
	}
}

#define STACK_SIZE (384 + CONFIG_TEST_EXTRA_STACK_SIZE)
#define THREADS_NUM 2

struct thread_info {
	k_tid_t tid;
	int executed;
	int priority;
	int cpu_id;
};
static struct thread_info tinfo[THREADS_NUM];
static struct k_thread tthread[THREADS_NUM];
static K_THREAD_STACK_ARRAY_DEFINE(tstack, THREADS_NUM, STACK_SIZE);


/**
 * @brief Test that the producer and consumer are indeed thread safe
 *
 * This can and should be validated on SMP machines where incoherent
 * memory could cause issues.
 */
ZTEST(rtio_spsc, test_spsc_threaded)
{

	tinfo[0].tid =
		k_thread_create(&tthread[0], tstack[0], STACK_SIZE,
				(k_thread_entry_t)t1_consume,
				&spsc, NULL, NULL,
				K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);
	tinfo[1].tid =
		k_thread_create(&tthread[1], tstack[1], STACK_SIZE,
				(k_thread_entry_t)t2_produce,
				&spsc, NULL, NULL,
				K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);

	k_thread_join(tinfo[1].tid, K_FOREVER);
	k_thread_join(tinfo[0].tid, K_FOREVER);
}

static struct rtio_mpsc push_pop_q;
static struct rtio_mpsc_node push_pop_nodes[2];

/*
 * @brief Push and pop one element
 *
 * @see rtio_mpsc_push(), rtio_mpsc_pop()
 *
 * @ingroup rtio_tests
 */
ZTEST(rtio_mpsc, test_push_pop)
{

	struct rtio_mpsc_node *node, *head, *stub, *next, *tail;

	rtio_mpsc_init(&push_pop_q);

	head = atomic_ptr_get(&push_pop_q.head);
	tail = push_pop_q.tail;
	stub = &push_pop_q.stub;
	next = atomic_ptr_get(&stub->next);

	zassert_equal(head, stub, "Head should point at stub");
	zassert_equal(tail, stub, "Tail should point at stub");
	zassert_is_null(next, "Next should be null");

	node = rtio_mpsc_pop(&push_pop_q);
	zassert_is_null(node, "Pop on empty queue should return null");

	rtio_mpsc_push(&push_pop_q, &push_pop_nodes[0]);

	head = atomic_ptr_get(&push_pop_q.head);

	zassert_equal(head, &push_pop_nodes[0], "Queue head should point at push_pop_node");
	next = atomic_ptr_get(&push_pop_nodes[0].next);
	zassert_is_null(next, NULL, "push_pop_node next should point at null");
	next = atomic_ptr_get(&push_pop_q.stub.next);
	zassert_equal(next, &push_pop_nodes[0], "Queue stub should point at push_pop_node");
	tail = push_pop_q.tail;
	stub = &push_pop_q.stub;
	zassert_equal(tail, stub, "Tail should point at stub");

	node = rtio_mpsc_pop(&push_pop_q);
	stub = &push_pop_q.stub;

	zassert_not_equal(node, stub, "Pop should not return stub");
	zassert_not_null(node, "Pop should not return null");
	zassert_equal(node, &push_pop_nodes[0],
		      "Pop should return push_pop_node %p, instead was %p",
		      &push_pop_nodes[0], node);

	node = rtio_mpsc_pop(&push_pop_q);
	zassert_is_null(node, "Pop on empty queue should return null");
}

#define MPSC_FREEQ_SZ 8
#define MPSC_ITERATIONS 100000
#define MPSC_STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACK_SIZE)
#define MPSC_THREADS_NUM 4

static struct thread_info mpsc_tinfo[MPSC_THREADS_NUM];
static struct k_thread mpsc_thread[MPSC_THREADS_NUM];
static K_THREAD_STACK_ARRAY_DEFINE(mpsc_stack, MPSC_THREADS_NUM, MPSC_STACK_SIZE);

struct mpsc_node {
	uint32_t id;
	struct rtio_mpsc_node n;
};


RTIO_SPSC_DECLARE(node_sq, struct mpsc_node, MPSC_FREEQ_SZ);

#define SPSC_INIT(n, sz) RTIO_SPSC_INITIALIZER(sz)

struct rtio_spsc_node_sq node_q[MPSC_THREADS_NUM] = {
	LISTIFY(MPSC_THREADS_NUM, SPSC_INIT, (,), MPSC_FREEQ_SZ)
};

static struct rtio_mpsc mpsc_q;

static void mpsc_consumer(void *p1, void *p2, void *p3)
{
	struct rtio_mpsc_node *n;
	struct mpsc_node *nn;

	for (int i = 0; i < (MPSC_ITERATIONS)*(MPSC_THREADS_NUM - 1); i++) {
		do {
			n = rtio_mpsc_pop(&mpsc_q);
			if (n == NULL) {
				k_yield();
			}
		} while (n == NULL);

		zassert_not_equal(n, &mpsc_q.stub, "mpsc should not produce stub");

		nn = CONTAINER_OF(n, struct mpsc_node, n);

		rtio_spsc_acquire(&node_q[nn->id]);
		rtio_spsc_produce(&node_q[nn->id]);
	}
}

static void mpsc_producer(void *p1, void *p2, void *p3)
{
	struct mpsc_node *n;
	uint32_t id = (uint32_t)(uintptr_t)p1;

	for (int i = 0; i < MPSC_ITERATIONS; i++) {
		do {
			n = rtio_spsc_consume(&node_q[id]);
			if (n == NULL) {
				k_yield();
			}
		} while (n == NULL);

		rtio_spsc_release(&node_q[id]);
		n->id = id;
		rtio_mpsc_push(&mpsc_q, &n->n);
	}
}

/**
 * @brief Test that the producer and consumer are indeed thread safe
 *
 * This can and should be validated on SMP machines where incoherent
 * memory could cause issues.
 */
ZTEST(rtio_mpsc, test_mpsc_threaded)
{
	rtio_mpsc_init(&mpsc_q);

	TC_PRINT("setting up mpsc producer free queues\n");
	/* Setup node free queues */
	for (int i = 0; i < MPSC_THREADS_NUM; i++) {
		for (int j = 0; j < MPSC_FREEQ_SZ; j++) {
			rtio_spsc_acquire(&node_q[i]);
		}
		rtio_spsc_produce_all(&node_q[i]);
	}

	TC_PRINT("starting consumer\n");
	mpsc_tinfo[0].tid =
		k_thread_create(&mpsc_thread[0], mpsc_stack[0], STACK_SIZE,
				(k_thread_entry_t)mpsc_consumer,
				NULL, NULL, NULL,
				K_PRIO_PREEMPT(5),
				K_INHERIT_PERMS, K_NO_WAIT);

	for (int i = 1; i < MPSC_THREADS_NUM; i++) {
		TC_PRINT("starting producer %i\n", i);
		mpsc_tinfo[i].tid =
			k_thread_create(&mpsc_thread[i], mpsc_stack[i], STACK_SIZE,
					(k_thread_entry_t)mpsc_producer,
					(void *)(uintptr_t)i, NULL, NULL,
					K_PRIO_PREEMPT(5),
					K_INHERIT_PERMS, K_NO_WAIT);
	}

	for (int i = 0; i < MPSC_THREADS_NUM; i++) {
		TC_PRINT("joining mpsc thread %d\n", i);
		k_thread_join(mpsc_tinfo[i].tid, K_FOREVER);
	}
}

RTIO_EXECUTOR_SIMPLE_DEFINE(simple_exec_simp);
RTIO_DEFINE(r_simple_simp, (struct rtio_executor *)&simple_exec_simp, 4, 4);

RTIO_EXECUTOR_CONCURRENT_DEFINE(simple_exec_con, 1);
RTIO_DEFINE(r_simple_con, (struct rtio_executor *)&simple_exec_con, 4, 4);

RTIO_IODEV_TEST_DEFINE(iodev_test_simple);

/**
 * @brief Test the basics of the RTIO API
 *
 * Ensures that we can setup an RTIO context, enqueue a request, and receive
 * a completion event.
 */
void test_rtio_simple_(struct rtio *r)
{
	int res;
	uintptr_t userdata[2] = {0, 1};
	struct rtio_sqe *sqe;
	struct rtio_cqe *cqe;

	rtio_iodev_test_init(&iodev_test_simple);

	TC_PRINT("setting up single no-op\n");
	sqe = rtio_spsc_acquire(r->sq);
	zassert_not_null(sqe, "Expected a valid sqe");
	rtio_sqe_prep_nop(sqe, (struct rtio_iodev *)&iodev_test_simple, &userdata[0]);

	TC_PRINT("submit with wait\n");
	res = rtio_submit(r, 1);
	zassert_ok(res, "Should return ok from rtio_execute");

	cqe = rtio_spsc_consume(r->cq);
	zassert_not_null(cqe, "Expected a valid cqe");
	zassert_ok(cqe->result, "Result should be ok");
	zassert_equal_ptr(cqe->userdata, &userdata[0], "Expected userdata back");
	rtio_spsc_release(r->cq);
}

ZTEST(rtio_api, test_rtio_simple)
{
	TC_PRINT("rtio simple simple\n");
	test_rtio_simple_(&r_simple_simp);
	TC_PRINT("rtio simple concurrent\n");
	test_rtio_simple_(&r_simple_con);
}

RTIO_EXECUTOR_SIMPLE_DEFINE(chain_exec_simp);
RTIO_DEFINE(r_chain_simp, (struct rtio_executor *)&chain_exec_simp, 4, 4);

RTIO_EXECUTOR_CONCURRENT_DEFINE(chain_exec_con, 1);
RTIO_DEFINE(r_chain_con, (struct rtio_executor *)&chain_exec_con, 4, 4);

RTIO_IODEV_TEST_DEFINE(iodev_test_chain0);
RTIO_IODEV_TEST_DEFINE(iodev_test_chain1);
struct rtio_iodev *iodev_test_chain[] = {&iodev_test_chain0, &iodev_test_chain1};

/**
 * @brief Test chained requests
 *
 * Ensures that we can setup an RTIO context, enqueue a chained requests,
 * and receive completion events in the correct order given the chained
 * flag and multiple devices where serialization isn't guaranteed.
 */
void test_rtio_chain_(struct rtio *r)
{
	int res;
	uintptr_t userdata[4] = {0, 1, 2, 3};
	struct rtio_sqe *sqe;
	struct rtio_cqe *cqe;

	for (int i = 0; i < 4; i++) {
		sqe = rtio_spsc_acquire(r->sq);
		zassert_not_null(sqe, "Expected a valid sqe");
		rtio_sqe_prep_nop(sqe, iodev_test_chain[i % 2],
				  &userdata[i]);
		sqe->flags |= RTIO_SQE_CHAINED;
	}

	/* Clear the last one */
	sqe->flags = 0;

	TC_PRINT("submitting\n");
	res = rtio_submit(r, 4);
	TC_PRINT("checking cq\n");
	zassert_ok(res, "Should return ok from rtio_execute");
	zassert_equal(rtio_spsc_consumable(r->cq), 4, "Should have 4 pending completions");

	for (int i = 0; i < 4; i++) {
		TC_PRINT("consume %d\n", i);
		cqe = rtio_spsc_consume(r->cq);
		zassert_not_null(cqe, "Expected a valid cqe");
		zassert_ok(cqe->result, "Result should be ok");
		zassert_equal_ptr(cqe->userdata, &userdata[i], "Expected in order completions");
		rtio_spsc_release(r->cq);
	}
}

ZTEST(rtio_api, test_rtio_chain)
{
	TC_PRINT("initializing iodev test devices\n");

	for (int i = 0; i < 2; i++) {
		rtio_iodev_test_init(iodev_test_chain[i]);
	}

	TC_PRINT("rtio chain simple\n");
	test_rtio_chain_(&r_chain_simp);
	TC_PRINT("rtio chain concurrent\n");
	test_rtio_chain_(&r_chain_con);
}


RTIO_EXECUTOR_SIMPLE_DEFINE(multi_exec_simp);
RTIO_DEFINE(r_multi_simp, (struct rtio_executor *)&multi_exec_simp, 4, 4);

RTIO_EXECUTOR_CONCURRENT_DEFINE(multi_exec_con, 2);
RTIO_DEFINE(r_multi_con, (struct rtio_executor *)&multi_exec_con, 4, 4);

RTIO_IODEV_TEST_DEFINE(iodev_test_multi0);
RTIO_IODEV_TEST_DEFINE(iodev_test_multi1);
struct rtio_iodev *iodev_test_multi[] = {&iodev_test_multi0, &iodev_test_multi1};

/**
 * @brief Test multiple asynchronous chains against one iodev
 */
void test_rtio_multiple_chains_(struct rtio *r)
{
	int res;
	uintptr_t userdata[4] = {0, 1, 2, 3};
	struct rtio_sqe *sqe;
	struct rtio_cqe *cqe;

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			sqe = rtio_spsc_acquire(r->sq);
			zassert_not_null(sqe, "Expected a valid sqe");
			rtio_sqe_prep_nop(sqe, iodev_test_multi[i],
					  (void *)userdata[i*2 + j]);
			if (j == 0) {
				sqe->flags |= RTIO_SQE_CHAINED;
			} else {
				sqe->flags |= 0;
			}
		}
	}

	TC_PRINT("calling submit from test case\n");
	res = rtio_submit(r, 0);
	zassert_ok(res, "Should return ok from rtio_execute");

	bool seen[4] = { 0 };

	TC_PRINT("waiting for 4 completions\n");
	for (int i = 0; i < 4; i++) {
		TC_PRINT("waiting on completion %d\n", i);
		cqe = rtio_spsc_consume(r->cq);

		while (cqe == NULL) {
			k_sleep(K_MSEC(1));
			cqe = rtio_spsc_consume(r->cq);
		}

		zassert_not_null(cqe, "Expected a valid cqe");
		TC_PRINT("result %d, would block is %d, inval is %d\n",
			 cqe->result, -EWOULDBLOCK, -EINVAL);
		zassert_ok(cqe->result, "Result should be ok");
		seen[(uintptr_t)cqe->userdata] = true;
		if (seen[1]) {
			zassert_true(seen[0], "Should see 0 before 1");
		}
		if (seen[3]) {
			zassert_true(seen[2], "Should see 2 before 3");
		}
		rtio_spsc_release(r->cq);
	}
}

ZTEST(rtio_api, test_rtio_multiple_chains)
{
	for (int i = 0; i < 2; i++) {
		rtio_iodev_test_init(iodev_test_multi[i]);
	}

	TC_PRINT("rtio multiple simple\n");
	test_rtio_multiple_chains_(&r_multi_simp);
	TC_PRINT("rtio_multiple concurrent\n");
	test_rtio_multiple_chains_(&r_multi_con);
}



#ifdef CONFIG_USERSPACE
K_APPMEM_PARTITION_DEFINE(rtio_partition);
K_APP_BMEM(rtio_partition) uint8_t syscall_bufs[4];
struct k_mem_domain rtio_domain;
#else
uint8_t syscall_bufs[4];
#endif

RTIO_EXECUTOR_SIMPLE_DEFINE(syscall_simple);
RTIO_DEFINE(r_syscall, (struct rtio_executor *)&syscall_simple, 4, 4);
RTIO_IODEV_TEST_DEFINE(iodev_test_syscall);

void rtio_syscall_test(void *p1, void *p2, void *p3)
{
	int res;
	struct rtio_sqe sqe;
	struct rtio_cqe cqe;

	struct rtio *r = &r_syscall;

	for (int i = 0; i < 4; i++) {
		TC_PRINT("copying sqe in from stack\n");
		/* Not really legal from userspace! Ugh */
		rtio_sqe_prep_nop(&sqe, &iodev_test_syscall,
				  &syscall_bufs[i]);
		res = rtio_sqe_copy_in(r, &sqe, 1);
		zassert_equal(res, 0, "Expected success copying sqe");
	}

	TC_PRINT("submitting\n");
	res = rtio_submit(r, 4);

	for (int i = 0; i < 4; i++) {
		TC_PRINT("consume %d\n", i);
		res = rtio_cqe_copy_out(r, &cqe, 1, K_FOREVER);
		zassert_equal(res, 1, "Expected success copying cqe");
		zassert_ok(cqe.result, "Result should be ok");
		zassert_equal_ptr(cqe.userdata, &syscall_bufs[i],
				  "Expected in order completions");
	}

}

#ifdef CONFIG_USERSPACE
ZTEST(rtio_api, test_rtio_syscalls_usermode)
{
	struct k_mem_partition *parts[] = {
#if Z_LIBC_PARTITION_EXISTS
		&z_libc_partition,
#endif
		&rtio_partition
	};

	TC_PRINT("syscalls from user mode test\n");
	TC_PRINT("test iodev init\n");
	rtio_iodev_test_init(&iodev_test_syscall);
	TC_PRINT("mem domain init\n");
	k_mem_domain_init(&rtio_domain, ARRAY_SIZE(parts), parts);
	TC_PRINT("mem domain add current\n");
	k_mem_domain_add_thread(&rtio_domain, k_current_get());
	TC_PRINT("rtio access grant\n");
	rtio_access_grant(&r_syscall, k_current_get());
	TC_PRINT("rtio iodev access grant, ptr %p\n", &iodev_test_syscall);
	k_object_access_grant(&iodev_test_syscall, k_current_get());
	TC_PRINT("user mode enter\n");
	k_thread_user_mode_enter(rtio_syscall_test, NULL, NULL, NULL);
}
#endif /* CONFIG_USERSPACE */


ZTEST(rtio_api, test_rtio_syscalls)
{
	TC_PRINT("test iodev init\n");
	rtio_iodev_test_init(&iodev_test_syscall);
	TC_PRINT("syscalls from kernel mode\n");
	rtio_syscall_test(NULL, NULL, NULL);
}




ZTEST_SUITE(rtio_spsc, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(rtio_mpsc, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(rtio_api, NULL, NULL, NULL, NULL, NULL);
