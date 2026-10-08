import packingsolver.onedimensional as pso

instance_builder = pso.InstanceBuilder()
instance_builder.set_objective(pso.Objective.BinPackingWithLeftovers)
instance_builder.add_bin_type(500, copies=10)
instance_builder.add_item_type(240, weight=200, copies=2)
instance_builder.add_item_type(160, weight=100, copies=3)
instance = instance_builder.build()
