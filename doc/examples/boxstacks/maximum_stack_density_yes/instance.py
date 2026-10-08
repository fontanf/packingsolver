import packingsolver.boxstacks as psbs

instance_builder = psbs.InstanceBuilder()
instance_builder.set_objective(psbs.Objective.BinPacking)
instance_builder.add_bin_type(
        7500,
        2400,
        3000,
        maximum_stack_density=0.6,
        copies=4,
)
instance_builder.add_item_type(2500, 1200, 1000, weight=700000, copies=9)
instance_builder.add_item_type(2500, 1200, 1000, weight=900000, copies=9)
instance = instance_builder.build()
