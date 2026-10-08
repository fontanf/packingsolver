import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPacking)
instance_builder.add_bin_type(7500, 2400, 3000, copies=2)
instance_builder.add_item_type(
        3000,
        2400,
        1500,
        weight=40,
        maximum_weight_above=30,
        copies=2,
)
instance_builder.add_item_type(
        3000,
        2400,
        1000,
        weight=20,
        maximum_weight_above=10000,
        copies=3,
)
instance = instance_builder.build()
