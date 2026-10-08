import packingsolver.rectangleguillotine as psg

instance_builder = psg.InstanceBuilder()
instance_builder.set_objective(psg.Objective.BinPackingWithLeftovers)
instance_builder.add_bin_type(1000, 700, copies=5)
instance_builder.add_item_type(250, 200, copies=2)
instance_builder.add_item_type(150, 300, copies=2)
instance_builder.add_item_type(200, 150, copies=3)
instance = instance_builder.build()
