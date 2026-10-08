import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPacking)
instance_builder.add_bin_type(
        13500,
        2440,
        2900,
        copies=2,
        semi_trailer_truck_parameters={
            "tractor_weight": 7808,
            "front_axle_middle_axle_distance": 3800,
            "front_axle_tractor_gravity_center_distance": 1040,
            "front_axle_harness_distance": 3330,
            "empty_trailer_weight": 7300,
            "harness_rear_axle_distance": 7630,
            "trailer_gravity_center_rear_axle_distance": 2350,
            "trailer_start_harness_distance": 1670,
            "rear_axle_maximum_weight": 31500,
            "middle_axle_maximum_weight": 12000,
        },
)
instance_builder.add_item_type(1200, 1000, 1200, weight=1200, copies=19)
instance_builder.add_item_type(1200, 1000, 800, weight=800, copies=19)
instance = instance_builder.build()
