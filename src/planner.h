/**
 * @file
 * Convert parsed queries to relational query plans.
 * Simple implementation, lacks real optimizer.
 * @author Henning Funke <henning.funke@cs.tu-dortmund.de>
 */
#pragma once




Expr* matchAndUnify ( Expr*   select,
                      Expr*   group ) {
    
    if ( traceMatch ( select, group ) ) {
        group->next = select->next;
        return group;
    }
    if ( select->child == nullptr ) {
        return select;
    }
    Expr* fc = select->child;
    select->child = matchAndUnify ( fc, group );
    Expr* prev = fc;
    Expr* c = fc->next;
    while ( c != nullptr ) {
        prev->next = matchAndUnify ( c, group );
        prev = c;
        c = c->next; 
    }
    return select;
}


ExprVec filterExpr ( Expr*    expr, 
                     bool   (*selector)(Expr*) ) {

    ExprVec res = {};
    if ( expr == nullptr ) return res; 
    if ( selector ( expr ) ) res.push_back ( expr );
    Expr* child = expr->child;
    while ( child != nullptr ) {
        ExprVec childRes = filterExpr ( child, selector );
        res.insert ( res.end(), childRes.begin(), childRes.end() );
        child = child->next;    
    }
    return res;
}


ExprVec collectConjunctionsInner ( Expr* expr ) {
    if ( expr == nullptr ) return {};
    ExprVec res = {};
    if ( expr->tag == Expr::AND ) {
        Expr* child = expr->child;
        while ( child != nullptr ) {
            if ( child->tag != Expr::AND ) {
                res.push_back ( child );
            }
            else {
                auto childRes = collectConjunctionsInner ( child );
                res.insert ( res.end(), childRes.begin(), childRes.end() );
            }
            child = child->next;
        }
    }  
    return res;
}


ExprVec collectChildrenOfTopLevelConjunctions ( Expr* expr ) {
    if ( expr == nullptr ) return {};
    if ( expr->tag != Expr::AND ) return { expr };
    return collectConjunctionsInner ( expr );
}


ExprVec filterExprVec ( ExprVec& vec, 
                        bool   (*selector)(Expr*) ) {

    ExprVec res = {};
    for ( Expr* elem : vec ) {
        ExprVec elemRes = filterExpr ( elem, selector );
        res.insert ( res.end(), elemRes.begin(), elemRes.end() );
    }
    return res;
}


void unifySelectAndGroupby ( ExprVec&  select, 
                             ExprVec&  groupby ) {

    for ( auto& grp : groupby ) {
        for ( size_t i = 0; i < select.size(); i++ ) {
            Expr* sel = select [i];
            select [i] = matchAndUnify ( sel, grp );
        }
    }
}


void buildCanonicalPlan ( ExprVec&   from, 
                          Expr*      where, 
                          Query&     query,
                          Database&  db ) {

    if ( from.size() > 0 ) {
       query.plan = query.planTables [ from[0]->symbol ].op;
    }

    /* cross product with all following relations */
    for ( size_t i = 1; i < from.size(); i++ ) {
        query.plan = 
            new NestedLoopsJoinOp ( nullptr,
                query.plan,
                query.planTables [ from[i]->symbol ].op
            );
    }
   
    /* Single selection near root */ 
    if ( where != nullptr ) {
        query.plan = new SelectionOp ( where, query.plan );
    }
}


bool isAndExpr ( Expr* expr ) {
    return expr->tag == Expr::AND;
}


std::vector < std::string > collectAttributes ( Expr* e ) {
    std::vector < std::string > res = {};
    if ( e->tag == Expr::ATTRIBUTE ) {
        res.push_back ( e->symbol );
    }
    Expr* c = e->child;
    while ( c != nullptr ) {
        auto childRes = collectAttributes ( c );
        res.insert ( res.end(), childRes.begin(), childRes.end() );
        c = c->next;
    }
    return res;
}



bool getNameOfMatchingTable ( std::vector < std::string >&  symbols,
                              Query&                        query,
                              std::string&                  result ) {

    /* Check if one table contains all symbols */
    for ( auto& tbl : query.planTables ) {
        Schema& s = tbl.second.schema;
        bool containsAll = true;
        for ( auto sym : symbols ) {
            if ( ! s.contains ( sym ) ) {
                containsAll = false;
                break;
            }
        }
        if ( containsAll ) {
            result = tbl.first;
            return true;
        } 
    }
    return false;
}




/* returns the remaining selection conditions */
ExprVec pushDownSelection ( ExprVec&   from, 
                            ExprVec&   where, 
                            Query&     query, 
                            Database&  db ) {

    ExprVec orphans;
    ExprVec remaining;
    for ( Expr* e : where ) {
        std::vector < std::string > symbols = collectAttributes ( e );
        if ( symbols.size() == 0 ) {
            orphans.push_back ( e );
            break;
        }
        std::string tableName = "";
        bool match = getNameOfMatchingTable ( symbols, query, tableName );
        if ( match ) {
            PlanOperator& op = query.planTables [ tableName ];
            /* add to existing selection operator.. */
            if ( op.op->tag == RelOperator::SELECTION ) {
                SelectionOp* sel = (SelectionOp*)op.op;
                sel->_condition = ExprGen::and_ ( sel->_condition, e );
            }
            /* ..or create selection operator */
            else {
                query.planPieces.erase ( op.op );
                op.op = { new SelectionOp ( e, op.op ) };
                query.planPieces.insert ( op.op );
            }
        }
        else {
            remaining.push_back ( e );
        }
    }
    return remaining;
}


struct JoinMapElement {
    Expr* a;
    Expr* b;
};



bool isUniqueAttribute ( Expr*      expr,
                         Database&  db ) {

    if ( expr->tag == Expr::ATTRIBUTE ) {
   
        if ( expr->symbol.compare ( "o_orderkey" ) == 0 ) {
            return true;
        }
        if ( expr->symbol.compare ( "p_partkey" ) == 0 ) {
            return true;
        }
        if ( expr->symbol.compare ( "s_suppkey" ) == 0 ) {
            return true;
        }
        if ( expr->symbol.compare ( "n_nationkey" ) == 0 ) {
            return true;
        }
        if ( expr->symbol.compare ( "r_regionkey" ) == 0 ) {
            return true;
        }
    }

    return false;
}


Expr* conjunction ( ExprVec vec ) {
    if ( vec.size() == 0 ) return nullptr;
    Expr* e = vec[0];
    for ( size_t i=1; i < vec.size(); i++ ) {
        e = ExprGen::and_ ( e, vec[i] );
    }
    return e;
}


typedef std::pair < std::string, std::string > RelationPair;
typedef std::pair < Expr*, Expr* > ExprPair;



/* returns the remaining selection conditions */
ExprVec addEqualityHashJoins ( ExprVec&   from, 
                               ExprVec&   where, 
                               Query&     query, 
                               Database&  db ) {
    ExprVec equalities;
    ExprVec remaining;
    for ( Expr* eq : where ) {
        if ( eq->tag != Expr::EQ ) {
            remaining.push_back ( eq );
            continue;
        }
        equalities.push_back ( eq );
    }

    std::map < RelationPair, std::vector <ExprPair> >  joinMap;

    /* Match equality conditions with tables in 'from'-clause */
    for ( Expr* eq : equalities ) {
        std::vector < std::string > leftAttr = collectAttributes ( eq->child );
        std::vector < std::string > rightAttr = collectAttributes ( eq->child->next ); 
        std::string nameA = "";
        std::string nameB = "";

        bool matchA = getNameOfMatchingTable ( leftAttr, query, nameA );
        bool matchB = getNameOfMatchingTable ( rightAttr, query, nameB );

        /* Could not find a single table that contains all attributes *
         * on one or both sides of the condition                      */
        if ( (!matchA) || (!matchB) ) {
            remaining.push_back ( eq );
            continue;
        }

        Expr* a = eq->child;
        Expr* b = eq->child->next;

        /* Move smaller relation to left (build) side */
        if ( db.relations[nameA].tupleNum() >= db.relations[nameB].tupleNum() ) {
            std::swap ( nameA, nameB );
            std::swap ( a, b );
        }

        joinMap [ std::make_pair ( nameA, nameB ) ].push_back ( std::make_pair ( a, b ) ); 
    }

    /* Compute join order */
    std::vector <std::pair <RelationPair, std::vector <ExprPair> > >  joinList;
    for ( auto& join : joinMap ) {
        joinList.push_back ( join );
    }
    std::sort ( 
        joinList.begin(), 
        joinList.end(),
        [&db,&query] ( const std::pair <RelationPair, std::vector <ExprPair> > a, 
                       const std::pair <RelationPair, std::vector <ExprPair> > b ) -> bool
        { 
            /* Compare two pairs of relations with their selection *
             * predicates ontop: A and B.                          */
            auto& relsA = a.first; // names
            auto& relsB = b.first; 
            RelOperator *opAR, *opAS, *opBR, *opBS; // get root of each plan piece
            opAR = query.planTables [ relsA.first ].op;
            opAS = query.planTables [ relsA.second ].op;
            opBR = query.planTables [ relsB.first ].op;
            opBS = query.planTables [ relsB.second ].op;
            int numSelectionsA = 0;
            int numSelectionsB = 0;
            if ( opAR->tag == RelOperator::SELECTION ) numSelectionsA++;
            if ( opAS->tag == RelOperator::SELECTION ) numSelectionsA++;
            if ( opBR->tag == RelOperator::SELECTION ) numSelectionsB++;
            if ( opBS->tag == RelOperator::SELECTION ) numSelectionsB++;
            
            if ( ( numSelectionsA > 0 || numSelectionsB > 0 )
               && (numSelectionsA != numSelectionsB ) ){
                /* Take pair that has more selection operators.   *
                 * (two at most i.e. one selection ontop of each  *
                 * relation)                                      */
                return numSelectionsA > numSelectionsB;
            } 
            else {
                /* Take pair with smaller right-hand size. *
                 * The right-hand relation already is the  *
                 * larger (probe) relation.                */
                return ( db.relations [ relsA.second ].tupleNum() < db.relations [ relsB.second ].tupleNum() );
            }
        }
    );

    /* Genrate hash joins for tables that were matched by equality conditions */
    for ( auto& join : joinList ) {
        std::string nameA = join.first.first;
        std::string nameB = join.first.second;
       
        bool singleMatch = false; 
        ExprVec cond;
        for ( auto eq : join.second ) {
            if ( isUniqueAttribute ( eq.first, db ) ) {
                singleMatch = true;
            }
            cond.push_back ( ExprGen::eq ( eq.first, eq.second ) ); 
            eq.second->next = nullptr;
        }

        RelOperator* a = query.planTables [ nameA ].op;
        RelOperator* b = query.planTables [ nameB ].op;

        /* already joined: add condition to existing join */
        if ( a == b ) {
            RelOperator* hj = query.planTables [ nameA ].op;
            if ( hj->tag != RelOperator::HASHJOIN ) {
                throw ResqlError ( "Planner expects operator to be hash join that is " + hj->name() );
            }
            ExprVec& eqs = ((HashJoinOp*)hj)->_equalities;
            eqs.insert ( eqs.begin(), cond.begin(), cond.end() );
            continue;
        }

        RelOperator* hj = (RelOperator*) new HashJoinOp ( cond, a, b );
        ((HashJoinOp*)hj)->_singleMatch = singleMatch;
        query.planTables [ nameA ].op = hj;
        query.planTables [ nameB ].op = hj;
        query.planPieces.insert ( hj );
        query.planPieces.erase ( a );
        query.planPieces.erase ( b );
        // update map tableName -> RelOperator*
        for ( auto& pt : query.planTables ) {
            if ( pt.second.op == a ) pt.second.op = hj;
            if ( pt.second.op == b ) pt.second.op = hj;
        }
        query.plan = hj;
    }
    return remaining;
}


std::map < std::string, SqlType > mapIdentifierTypes ( Database& db ) {
    std::map < std::string, SqlType > res;
    for ( auto const& rel : db.relations ) {
        for ( auto const& att : rel.second._schema._attribs ) {
            res[att.name] = att.type; 
        }

    }
    return res;
}




void buildQuery ( Query& query, Database& db ) {
    
    /* Prepare query elements */
    ExprVec select  = exprListToVector ( query.selectExpr ); 
    ExprVec from    = exprListToVector ( query.fromExpr ); 
    ExprVec where   = collectChildrenOfTopLevelConjunctions ( query.whereExpr );
    ExprVec groupby = exprListToVector ( query.groupbyExpr ); 
    ExprVec orderby = exprListToVector ( query.orderbyExpr );
   
    if ( query.selectExpr->tag == Expr::STAR ) {
        query.requestAll = true;
        if ( from.size() == 0 ) throw ResqlError ( "Need from-clause for 'select *'" );     
    }    

    /* Use same Expr* in 'select' and 'groupby' clause */
    unifySelectAndGroupby ( select, groupby );
    
    /* Use same Expr* in 'select' and 'orderby' clause */
    // todo:

    /* Extract aggregations from 'select' caluse */
    ExprVec aggregations = filterExprVec ( select, &isAggregationExpr );

    /* add scans */
    for ( Expr* e : from ) {
        std::string tblName = e->symbol;
        if ( db.relations.count ( tblName ) == 0 ) {
            throw ResqlError ( "Table " + tblName + " does not exist." );
        }
        Relation& rel = db.relations [ tblName ];
        PlanOperator planOp = { new ScanOp ( &rel, tblName ), rel._schema };
        query.planTables [ tblName ] = planOp;
        query.planPieces.insert ( planOp.op );
    }

    /* Try to push selection predicates to each scan *
     * and retrieve the remaining 'where' conditions */
    where = pushDownSelection ( from, where, query, db );

    /* Creates plan pieces with hash joins */
    where = addEqualityHashJoins ( from, where, query, db );

    /* Combine plan pieces that were not joined yet *
     * with nested loops.                           */
    if ( query.planPieces.size() > 0 ) {
        auto it = query.planPieces.begin();
        /* shows plan pieces */
        query.plan = *it;
        it++;
        while ( it != query.planPieces.end() ) {
            /* shows plan pieces */
            query.plan = new NestedLoopsJoinOp ( nullptr, query.plan, *it );
            it++;
        }
    }
        
    /* Add remaining where conditions                 *
     * (excl. push-down selection and join equalities */
    if ( where.size() > 0 ) {
        query.plan = new SelectionOp ( conjunction ( where ), query.plan );
    }

    /* group by clause */
    if ( groupby.size() > 0 || aggregations.size() > 0 ) {
        query.plan = new AggregationOp ( aggregations, groupby, query.plan );
    }

    /* Select clause */
    if ( query.selectExpr != nullptr ) {
        if ( query.selectExpr->tag != Expr::STAR ) {
            query.plan = new ProjectionOp ( select, query.plan );
        }
    }
    
    /* order by clause */
    if (orderby.empty() == false) {
        query.plan = new OrderByOp (std::move(orderby), query.plan);
    }
   
    /* result materialization */ 
    if ( !query.plan->isMaterializedOperator() ) {
        query.plan = new MaterializeOp ( query.plan );
    }
       
    /* limit clause */ 
    if ( query.useLimit ) {
        query.plan->addLimit ( query.limit );
    } 
}
