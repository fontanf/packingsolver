import packingsolver.rectangle as psr

instance_builder = psr.InstanceBuilder()
instance_builder.set_objective(psr.Objective.BinPackingWithLeftovers)
instance_builder.add_bin_type(700, 400, copies=2)
instance_builder.add_item_type(200, 200)
instance_builder.add_item_type(400, 400, group_id=1)
instance_builder.add_item_type(150, 150, group_id=2)
instance = instance_builder.build()
