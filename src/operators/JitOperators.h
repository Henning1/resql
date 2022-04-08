/**
 * @file
 * Translate relational operators to flounder IR.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once


#include "RelOperator.h"
#include "JitContextFlounder.h"
#include "ExpressionsJitFlounder.h"
#include "ValuesJitFlounder.h"
#include "dbdata.h"
#include "qlib/qlib.h"

#include "scan.h"
#include "projection.h"
#include "selection.h"
#include "materialize.h"
#include "nestedloopsjoin.h"
#include "hashjoin.h"
#include "aggregation.h"
#include "orderby.h"
