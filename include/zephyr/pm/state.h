/*
 * Copyright (c) 2020 Intel corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_PM_STATE_H_
#define ZEPHYR_INCLUDE_PM_STATE_H_

#include <zephyr/sys/util.h>
#include <zephyr/devicetree.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System Power Management States
 * @defgroup subsys_pm_states States
 * @ingroup subsys_pm
 * @{
 */

/**
 * @enum pm_state Power management state
 */
enum pm_state {
	/**
	 * @brief Runtime active state
	 *
	 * The system is fully powered and active.
	 *
	 * @note This state is correlated with ACPI G0/S0 state
	 */
	PM_STATE_ACTIVE,
	/**
	 * @brief Runtime idle state
	 *
	 * Runtime idle is a system sleep state in which all of the cores
	 * enter deepest possible idle state and wait for interrupts, no
	 * requirements for the devices, leaving them at the states where
	 * they are.
	 *
	 * @note This state is correlated with ACPI S0ix state
	 */
	PM_STATE_RUNTIME_IDLE,
	/**
	 * @brief Suspend to idle state
	 *
	 * The system goes through a normal platform suspend where it puts
	 * all of the cores in deepest possible idle state and *may* puts
	 * peripherals into low-power states. No operating state is lost (ie.
	 * the cpu core does not lose execution context), so the system can go
	 * back to where it left off easily enough.
	 *
	 * @note This state is correlated with ACPI S1 state
	 */
	PM_STATE_SUSPEND_TO_IDLE,
	/**
	 * @brief Standby state
	 *
	 * In addition to putting peripherals into low-power states all
	 * non-boot CPUs are powered off. It should allow more energy to be
	 * saved relative to suspend to idle, but the resume latency will
	 * generally be greater than for that state. But it should be the same
	 * state with suspend to idle state on uniprocessor system.
	 *
	 * @note This state is correlated with ACPI S2 state
	 */
	PM_STATE_STANDBY,
	/**
	 * @brief Suspend to ram state
	 *
	 * This state offers significant energy savings by powering off as much
	 * of the system as possible, where memory should be placed into the
	 * self-refresh mode to retain its contents. The state of devices and
	 * CPUs is saved and held in memory, and it may require some boot-
	 * strapping code in ROM to resume the system from it.
	 *
	 * @note This state is correlated with ACPI S3 state
	 */
	PM_STATE_SUSPEND_TO_RAM,
	/**
	 * @brief Suspend to disk state
	 *
	 * This state offers significant energy savings by powering off as much
	 * of the system as possible, including the memory. The contents of
	 * memory are written to disk or other non-volatile storage, and on
	 * resume it's read back into memory with the help of boot-strapping
	 * code, restores the system to the same point of execution where it
	 * went to suspend to disk.
	 *
	 * @note This state is correlated with ACPI S4 state
	 */
	PM_STATE_SUSPEND_TO_DISK,
	/**
	 * @brief Soft off state
	 *
	 * This state consumes a minimal amount of power and requires a large
	 * latency in order to return to runtime active state. The contents of
	 * system(CPU and memory) will not be preserved, so the system will be
	 * restarted as if from initial power-up and kernel boot.
	 *
	 * @note This state is correlated with ACPI G2/S5 state
	 */
	PM_STATE_SOFT_OFF,
	/** Number of power management states (internal use) */
	PM_STATE_COUNT,
};

/**
 * Information about a power management state
 */
struct pm_state_info {
	enum pm_state state;

	/**
	 * Some platforms have multiple states that map to
	 * one Zephyr power state. This property allows the platform
	 * distinguish them. e.g:
	 *
	 * @code{.dts}
	 *	idle-states {
	 *		state0: state0 {
	 *			compatible = "zephyr,idle-state";
	 *			idle-state-name = "suspend-to-idle";
	 *			zephyr,substate-id = <1>;
	 *			min-residency-us = <10000>;
	 *			exit-latency-us = <100>;
	 *		};
	 *		state1: state1 {
	 *			compatible = "zephyr,idle-state";
	 *			idle-state-name = "suspend-to-idle";
	 *			zephyr,substate-id = <2>;
	 *			min-residency-us = <20000>;
	 *			exit-latency-us = <200>;
	 *		};
	 *	};
	 * @endcode
	 */
	uint8_t substate_id;

	/**
	 * Minimum residency duration in microseconds. It is the minimum
	 * time for a given idle state to be worthwhile energywise.
	 *
	 * @note 0 means that this property is not available for this state.
	 */
	uint32_t min_residency_us;

	/**
	 * Worst case latency in microseconds required to exit the idle state.
	 *
	 * @note 0 means that this property is not available for this state.
	 */
	uint32_t exit_latency_us;
};

/** @cond INTERNAL_HIDDEN */

/**
 * @brief Helper macro to initialize an entry of a struct pm_state_info array
 * when using UTIL_LISTIFY in PM_STATE_INFO_LIST_FROM_DT_CPU.
 *
 * @param i UTIL_LISTIFY entry index.
 * @param node_id A node identifier with compatible zephyr,idle-state
 */
#define Z_PM_STATE_INFO_FROM_DT_CPU(i, node_id) \
	PM_STATE_INFO_DT_INIT(DT_PHANDLE_BY_IDX(node_id, cpu_idle_states, i))

/**
 * @brief Helper macro to initialize an entry of a struct pm_state array when
 * using UTIL_LISTIFY in PM_STATE_LIST_FROM_DT_CPU.
 *
 * @param i UTIL_LISTIFY entry index.
 * @param node_id A node identifier with compatible zephyr,idle-state
 */
#define Z_PM_STATE_FROM_DT_CPU(i, node_id) \
	PM_STATE_DT_INIT(DT_PHANDLE_BY_IDX(node_id, cpu_idle_states, i))

/** @endcond */

/**
 * @brief Initializer for struct pm_state_info given a DT node identifier with
 * zephyr,idle-state compatible.
 *
 * @param node_id A node identifier with compatible zephyr,idle-state
 */
#define PM_STATE_INFO_DT_INIT(node_id)					       \
	{								       \
		.state = PM_STATE_DT_INIT(node_id),			       \
		.substate_id = DT_PROP_OR(node_id, zephyr_substate_id, 0),     \
		.min_residency_us = DT_PROP_OR(node_id, min_residency_us, 0),  \
		.exit_latency_us = DT_PROP_OR(node_id, exit_latency_us, 0),    \
	}

/**
 * @brief Initializer for enum pm_state given a DT node identifier with
 * zephyr,idle-state compatible.
 *
 * @param node_id A node identifier with compatible zephyr,idle-state
 */
#define PM_STATE_DT_INIT(node_id) \
	DT_ENUM_IDX(node_id, idle_state_name)

/**
 * @brief Obtain number of CPU power states supported by the given CPU node
 * identifier.
 *
 * @param node_id A CPU node identifier.
 * @return Number of supported CPU power states.
 */
#define DT_NUM_CPU_POWER_STATES(node_id) \
	DT_PROP_LEN_OR(node_id, cpu_idle_states, 0)

/**
 * @brief Initialize an array of struct pm_state_info with information from all
 * the states present in the given CPU node identifier.
 *
 * Example devicetree fragment:
 *
 * @code{.dts}
 *	cpus {
 *		...
 *		cpu0: cpu@0 {
 *			device_type = "cpu";
 *			...
 *			cpu-idle-states = <&state0 &state1>;
 *		};
 *
 *		idle-states {
 *			state0: state0 {
 *				compatible = "zephyr,idle-state";
 *				idle-state-name = "suspend-to-idle";
 *				min-residency-us = <10000>;
 *				exit-latency-us = <100>;
 *			};
 *
 *			state1: state1 {
 *				compatible = "zephyr,idle-state";
 *				idle-state-name = "suspend-to-ram";
 *				min-residency-us = <50000>;
 *				exit-latency-us = <500>;
 *			};
 *		};
 *	};

 * @endcode
 *
 * Example usage:
 *
 * @code{.c}
 * const struct pm_state_info states[] =
 *	PM_STATE_INFO_LIST_FROM_DT_CPU(DT_NODELABEL(cpu0));
 * @endcode
 *
 * @param node_id A CPU node identifier.
 */
#define PM_STATE_INFO_LIST_FROM_DT_CPU(node_id)				       \
	{								       \
		LISTIFY(DT_NUM_CPU_POWER_STATES(node_id),		       \
			Z_PM_STATE_INFO_FROM_DT_CPU, (,), node_id)	       \
	}

/**
 * @brief Initialize an array of struct pm_state with information from all the
 * states present in the given CPU node identifier.
 *
 * Example devicetree fragment:
 *
 * @code{.dts}
 *	cpus {
 *		...
 *		cpu0: cpu@0 {
 *			device_type = "cpu";
 *			...
 *			cpu-idle-states = <&state0 &state1>;
 *		};
 *
 *		idle-states {
 *			state0: state0 {
 *				compatible = "zephyr,idle-state";
 *				idle-state-name = "suspend-to-idle";
 *				min-residency-us = <10000>;
 *				exit-latency-us = <100>;
 *			};
 *
 *			state1: state1 {
 *				compatible = "zephyr,idle-state";
 *				idle-state-name = "suspend-to-ram";
 *				min-residency-us = <50000>;
 *				exit-latency-us = <500>;
 *			};
 *		};
 *	};
 * @endcode
 *
 * Example usage:
 *
 * @code{.c}
 * const enum pm_state states[] = PM_STATE_LIST_FROM_DT_CPU(DT_NODELABEL(cpu0));
 * @endcode
 *
 * @param node_id A CPU node identifier.
 */
#define PM_STATE_LIST_FROM_DT_CPU(node_id)				       \
	{								       \
		LISTIFY(DT_NUM_CPU_POWER_STATES(node_id),		       \
			Z_PM_STATE_FROM_DT_CPU, (,), node_id)		       \
	}


/**
 * Obtain information about all supported states by a CPU.
 *
 * @param cpu CPU index.
 * @param states Where to store the list of supported states.
 *
 * @return Number of supported states.
 */
uint8_t pm_state_cpu_get_all(uint8_t cpu, const struct pm_state_info **states);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
