bulk insert lineitem from "tpch/datasets/sf01/lineitem.tbl" with ( fieldterminator="|" );
bulk insert orders   from "tpch/datasets/sf01/orders.tbl"   with ( fieldterminator="|" );
bulk insert customer from "tpch/datasets/sf01/customer.tbl" with ( fieldterminator="|" );
bulk insert partsupp from "tpch/datasets/sf01/partsupp.tbl" with ( fieldterminator="|" );
bulk insert supplier from "tpch/datasets/sf01/supplier.tbl" with ( fieldterminator="|" );
bulk insert part     from "tpch/datasets/sf01/part.tbl"     with ( fieldterminator="|" );
bulk insert nation   from "tpch/datasets/sf01/nation.tbl"   with ( fieldterminator="|" );
bulk insert region   from "tpch/datasets/sf01/region.tbl"   with ( fieldterminator="|" );

