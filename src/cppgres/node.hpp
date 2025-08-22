#pragma once
#include <concepts>
#include <type_traits>

#include "imports.h"
#include "utils/pfr.hpp"

#if PG_MAJORVERSION_NUM < 16
typedef bool (*tree_walker_callback)(Node *node, void *context);
#endif

namespace cppgres {

using node_tag = ::NodeTag;

template <typename T>
concept node_tagged = std::is_standard_layout_v<T> && requires(T t) {
  { t.type } -> std::convertible_to<node_tag>;
} && (offsetof(T, type) == 0);

template <typename T>
concept node_xpr_tagged = std::is_standard_layout_v<T> && requires(T t) {
  { t.xpr } -> std::convertible_to<::Expr>;
} && (offsetof(T, xpr) == 0);

template <typename T>
concept node_inherited_base = std::is_standard_layout_v<std::remove_cvref_t<T>> && requires(T t) {
  requires node_tagged<std::remove_cvref_t<decltype(boost::pfr::get<0>(t))>> ||
               node_xpr_tagged<std::remove_cvref_t<decltype(boost::pfr::get<0>(t))>>;
};

template <typename T>
concept node_inherited = std::is_standard_layout_v<std::remove_cvref_t<T>> && requires(T t) {
  requires node_tagged<std::remove_cvref_t<decltype(boost::pfr::get<0>(t))>> ||
               node_xpr_tagged<std::remove_cvref_t<decltype(boost::pfr::get<0>(t))>> ||
               node_inherited_base<std::remove_cvref_t<decltype(boost::pfr::get<0>(t))>>;
};

template <typename T>
concept node = node_tagged<T> || node_xpr_tagged<T> || node_inherited<T>;

template <typename T> struct node_traits {
  static inline bool is(auto &node) { return false; }
};
template <node_tag T> struct node_tag_traits;
template <typename T> struct node_coverage;

#define node_mapping(name)                                                                         \
  namespace nodes {                                                                                \
  struct name {                                                                                    \
    using underlying_type = ::name;                                                                \
    static constexpr inline node_tag tag = T_##name;                                               \
    name(underlying_type v) : val(v) {}                                                            \
    name() { reinterpret_cast<node_tag &>(val) = T_##name; }                                       \
    underlying_type &as_ref() { return val; }                                                      \
    underlying_type *as_ptr() { return &val; }                                                     \
                                                                                                   \
  private:                                                                                         \
    [[maybe_unused]] underlying_type val{};                                                        \
  };                                                                                               \
  }                                                                                                \
  static_assert((sizeof(name) == sizeof(::name)) && (alignof(name) == alignof(::name)));           \
  static_assert(std::is_standard_layout_v<name>);                                                  \
  static_assert(std::is_aggregate_v<name>);                                                        \
  template <> struct node_coverage<::name> {                                                       \
    using type = nodes::name;                                                                      \
  };                                                                                               \
  template <> struct node_tag_traits<node_tag::T_##name> {                                         \
    using type = nodes::name;                                                                      \
  };                                                                                               \
  template <> struct node_traits<nodes::name> {                                                    \
    static inline constexpr node_tag tag = node_tag::T_##name;                                     \
    static inline bool is(::Node *node) {                                                          \
      return *reinterpret_cast<node_tag *>(node) == node_tag::T_##name;                            \
    }                                                                                              \
    static inline bool is(::name *node) {                                                          \
      return *reinterpret_cast<node_tag *>(node) == node_tag::T_##name;                            \
    }                                                                                              \
    static inline bool is(void *node) {                                                            \
      return *reinterpret_cast<node_tag *>(node) == node_tag::T_##name;                            \
    }                                                                                              \
    static inline bool is(nodes::name *node) { return true; }                                      \
    static inline bool is(nodes::name &node) { return true; }                                      \
    static inline bool is(auto &node) { return false; }                                            \
    static inline nodes::name *allocate(abstract_memory_context &&ctx = memory_context()) {        \
      auto ptr = ctx.alloc<nodes::name>();                                                         \
      reinterpret_cast<node_tag &>(ptr) = tag;                                                     \
      return ptr;                                                                                  \
    }                                                                                              \
  }

#define node_mapping_no_node_traits(name)                                                          \
  namespace nodes {                                                                                \
  using name = node_coverage<::name>::type;                                                        \
  }                                                                                                \
  template <> struct node_tag_traits<node_tag::T_##name> {                                         \
    using type = nodes::name;                                                                      \
    static_assert(::cppgres::node<name>);                                                          \
  }

node_mapping(List);
node_mapping(Alias);
node_mapping(RangeVar);
node_mapping(TableFunc);
node_mapping(IntoClause);
node_mapping(Var);
node_mapping(Const);
node_mapping(Param);
node_mapping(Aggref);
node_mapping(GroupingFunc);
node_mapping(WindowFunc);
#if PG_MAJORVERSION_NUM >= 17
node_mapping(WindowFuncRunCondition);
node_mapping(MergeSupportFunc);
#endif
node_mapping(SubscriptingRef);
node_mapping(FuncExpr);
node_mapping(NamedArgExpr);
node_mapping(OpExpr);
node_mapping_no_node_traits(DistinctExpr);
node_mapping_no_node_traits(NullIfExpr);
node_mapping(ScalarArrayOpExpr);
node_mapping(BoolExpr);
node_mapping(SubLink);
node_mapping(SubPlan);
node_mapping(AlternativeSubPlan);
node_mapping(FieldSelect);
node_mapping(FieldStore);
node_mapping(RelabelType);
node_mapping(CoerceViaIO);
node_mapping(ArrayCoerceExpr);
node_mapping(ConvertRowtypeExpr);
node_mapping(CollateExpr);
node_mapping(CaseExpr);
node_mapping(CaseWhen);
node_mapping(CaseTestExpr);
node_mapping(ArrayExpr);
node_mapping(RowExpr);
node_mapping(RowCompareExpr);
node_mapping(CoalesceExpr);
node_mapping(MinMaxExpr);
node_mapping(SQLValueFunction);
node_mapping(XmlExpr);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(JsonFormat);
node_mapping(JsonReturning);
node_mapping(JsonValueExpr);
node_mapping(JsonConstructorExpr);
node_mapping(JsonIsPredicate);
#endif
#if PG_MAJORVERSION_NUM >= 17
node_mapping(JsonBehavior);
node_mapping(JsonExpr);
node_mapping(JsonTablePath);
node_mapping(JsonTablePathScan);
node_mapping(JsonTableSiblingJoin);
#endif
node_mapping(NullTest);
node_mapping(BooleanTest);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(MergeAction);
#endif
node_mapping(CoerceToDomain);
node_mapping(CoerceToDomainValue);
node_mapping(SetToDefault);
node_mapping(CurrentOfExpr);
node_mapping(NextValueExpr);
node_mapping(InferenceElem);
#if PG_MAJORVERSION_NUM >= 18
node_mapping(ReturningExpr);
#endif
node_mapping(TargetEntry);
node_mapping(RangeTblRef);
node_mapping(JoinExpr);
node_mapping(FromExpr);
node_mapping(OnConflictExpr);
node_mapping(Query);
node_mapping(TypeName);
node_mapping(ColumnRef);
node_mapping(ParamRef);
node_mapping(A_Expr);
node_mapping(A_Const);
node_mapping(TypeCast);
node_mapping(CollateClause);
node_mapping(RoleSpec);
node_mapping(FuncCall);
node_mapping(A_Star);
node_mapping(A_Indices);
node_mapping(A_Indirection);
node_mapping(A_ArrayExpr);
node_mapping(ResTarget);
node_mapping(MultiAssignRef);
node_mapping(SortBy);
node_mapping(WindowDef);
node_mapping(RangeSubselect);
node_mapping(RangeFunction);
node_mapping(RangeTableFunc);
node_mapping(RangeTableFuncCol);
node_mapping(RangeTableSample);
node_mapping(ColumnDef);
node_mapping(TableLikeClause);
node_mapping(IndexElem);
node_mapping(DefElem);
node_mapping(LockingClause);
node_mapping(XmlSerialize);
node_mapping(PartitionElem);
#if PG_MAJORVERSION_NUM == 17
node_mapping(SinglePartitionSpec);
#endif
node_mapping(PartitionSpec);
node_mapping(PartitionBoundSpec);
node_mapping(PartitionRangeDatum);
node_mapping(PartitionCmd);
node_mapping(RangeTblEntry);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(RTEPermissionInfo);
#endif
node_mapping(RangeTblFunction);
node_mapping(TableSampleClause);
node_mapping(WithCheckOption);
node_mapping(SortGroupClause);
node_mapping(GroupingSet);
node_mapping(WindowClause);
node_mapping(RowMarkClause);
node_mapping(WithClause);
node_mapping(InferClause);
node_mapping(OnConflictClause);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(CTESearchClause);
node_mapping(CTECycleClause);
#endif
node_mapping(CommonTableExpr);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(MergeWhenClause);
#endif
#if PG_MAJORVERSION_NUM >= 18
node_mapping(ReturningOption);
node_mapping(ReturningClause);
#endif
node_mapping(TriggerTransition);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(JsonOutput);
#endif
#if PG_MAJORVERSION_NUM >= 17
node_mapping(JsonArgument);
node_mapping(JsonFuncExpr);
node_mapping(JsonTablePathSpec);
node_mapping(JsonTable);
node_mapping(JsonTableColumn);
node_mapping(JsonKeyValue);
node_mapping(JsonParseExpr);
node_mapping(JsonScalarExpr);
node_mapping(JsonSerializeExpr);
#endif
#if PG_MAJORVERSION_NUM >= 16
node_mapping(JsonObjectConstructor);
node_mapping(JsonArrayConstructor);
node_mapping(JsonArrayQueryConstructor);
node_mapping(JsonAggConstructor);
node_mapping(JsonObjectAgg);
node_mapping(JsonArrayAgg);
#endif
node_mapping(RawStmt);
node_mapping(InsertStmt);
node_mapping(DeleteStmt);
node_mapping(UpdateStmt);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(MergeStmt);
#endif
node_mapping(SelectStmt);
node_mapping(SetOperationStmt);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(ReturnStmt);
#endif
#if PG_MAJORVERSION_NUM >= 14
node_mapping(PLAssignStmt);
#endif
node_mapping(CreateSchemaStmt);
node_mapping(AlterTableStmt);
node_mapping(AlterTableCmd);
#if PG_MAJORVERSION_NUM >= 18
node_mapping(ATAlterConstraint);
#endif
node_mapping(ReplicaIdentityStmt);
node_mapping(AlterCollationStmt);
node_mapping(AlterDomainStmt);
node_mapping(GrantStmt);
node_mapping(ObjectWithArgs);
node_mapping(AccessPriv);
node_mapping(GrantRoleStmt);
node_mapping(AlterDefaultPrivilegesStmt);
node_mapping(CopyStmt);
node_mapping(VariableSetStmt);
node_mapping(VariableShowStmt);
node_mapping(CreateStmt);
node_mapping(Constraint);
node_mapping(CreateTableSpaceStmt);
node_mapping(DropTableSpaceStmt);
node_mapping(AlterTableSpaceOptionsStmt);
node_mapping(AlterTableMoveAllStmt);
node_mapping(CreateExtensionStmt);
node_mapping(AlterExtensionStmt);
node_mapping(AlterExtensionContentsStmt);
node_mapping(CreateFdwStmt);
node_mapping(AlterFdwStmt);
node_mapping(CreateForeignServerStmt);
node_mapping(AlterForeignServerStmt);
node_mapping(CreateForeignTableStmt);
node_mapping(CreateUserMappingStmt);
node_mapping(AlterUserMappingStmt);
node_mapping(DropUserMappingStmt);
node_mapping(ImportForeignSchemaStmt);
node_mapping(CreatePolicyStmt);
node_mapping(AlterPolicyStmt);
node_mapping(CreateAmStmt);
node_mapping(CreateTrigStmt);
node_mapping(CreateEventTrigStmt);
node_mapping(AlterEventTrigStmt);
node_mapping(CreatePLangStmt);
node_mapping(CreateRoleStmt);
node_mapping(AlterRoleStmt);
node_mapping(AlterRoleSetStmt);
node_mapping(DropRoleStmt);
node_mapping(CreateSeqStmt);
node_mapping(AlterSeqStmt);
node_mapping(DefineStmt);
node_mapping(CreateDomainStmt);
node_mapping(CreateOpClassStmt);
node_mapping(CreateOpClassItem);
node_mapping(CreateOpFamilyStmt);
node_mapping(AlterOpFamilyStmt);
node_mapping(DropStmt);
node_mapping(TruncateStmt);
node_mapping(CommentStmt);
node_mapping(SecLabelStmt);
node_mapping(DeclareCursorStmt);
node_mapping(ClosePortalStmt);
node_mapping(FetchStmt);
node_mapping(IndexStmt);
node_mapping(CreateStatsStmt);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(StatsElem);
#endif
node_mapping(AlterStatsStmt);
node_mapping(CreateFunctionStmt);
node_mapping(FunctionParameter);
node_mapping(AlterFunctionStmt);
node_mapping(DoStmt);
node_mapping(InlineCodeBlock);
node_mapping(CallStmt);
node_mapping(CallContext);
node_mapping(RenameStmt);
node_mapping(AlterObjectDependsStmt);
node_mapping(AlterObjectSchemaStmt);
node_mapping(AlterOwnerStmt);
node_mapping(AlterOperatorStmt);
node_mapping(AlterTypeStmt);
node_mapping(RuleStmt);
node_mapping(NotifyStmt);
node_mapping(ListenStmt);
node_mapping(UnlistenStmt);
node_mapping(TransactionStmt);
node_mapping(CompositeTypeStmt);
node_mapping(CreateEnumStmt);
node_mapping(CreateRangeStmt);
node_mapping(AlterEnumStmt);
node_mapping(ViewStmt);
node_mapping(LoadStmt);
node_mapping(CreatedbStmt);
node_mapping(AlterDatabaseStmt);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(AlterDatabaseRefreshCollStmt);
#endif
node_mapping(AlterDatabaseSetStmt);
node_mapping(DropdbStmt);
node_mapping(AlterSystemStmt);
node_mapping(ClusterStmt);
node_mapping(VacuumStmt);
node_mapping(VacuumRelation);
node_mapping(ExplainStmt);
node_mapping(CreateTableAsStmt);
node_mapping(RefreshMatViewStmt);
node_mapping(CheckPointStmt);
node_mapping(DiscardStmt);
node_mapping(LockStmt);
node_mapping(ConstraintsSetStmt);
node_mapping(ReindexStmt);
node_mapping(CreateConversionStmt);
node_mapping(CreateCastStmt);
node_mapping(CreateTransformStmt);
node_mapping(PrepareStmt);
node_mapping(ExecuteStmt);
node_mapping(DeallocateStmt);
node_mapping(DropOwnedStmt);
node_mapping(ReassignOwnedStmt);
node_mapping(AlterTSDictionaryStmt);
node_mapping(AlterTSConfigurationStmt);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(PublicationTable);
node_mapping(PublicationObjSpec);
#endif
node_mapping(CreatePublicationStmt);
node_mapping(AlterPublicationStmt);
node_mapping(CreateSubscriptionStmt);
node_mapping(AlterSubscriptionStmt);
node_mapping(DropSubscriptionStmt);
node_mapping(PlannerGlobal);
node_mapping(PlannerInfo);
node_mapping(RelOptInfo);
node_mapping(IndexOptInfo);
node_mapping(ForeignKeyOptInfo);
node_mapping(StatisticExtInfo);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(JoinDomain);
#endif
node_mapping(EquivalenceClass);
node_mapping(EquivalenceMember);
node_mapping(PathKey);
#if PG_MAJORVERSION_NUM >= 17
node_mapping(GroupByOrdering);
#endif
node_mapping(PathTarget);
node_mapping(ParamPathInfo);
node_mapping(Path);
node_mapping(IndexPath);
node_mapping(IndexClause);
node_mapping(BitmapHeapPath);
node_mapping(BitmapAndPath);
node_mapping(BitmapOrPath);
node_mapping(TidPath);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(TidRangePath);
#endif
node_mapping(SubqueryScanPath);
node_mapping(ForeignPath);
node_mapping(CustomPath);
node_mapping(AppendPath);
node_mapping(MergeAppendPath);
node_mapping(GroupResultPath);
node_mapping(MaterialPath);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(MemoizePath);
#endif
node_mapping(UniquePath);
node_mapping(GatherPath);
node_mapping(GatherMergePath);
node_mapping(NestPath);
node_mapping(MergePath);
node_mapping(HashPath);
node_mapping(ProjectionPath);
node_mapping(ProjectSetPath);
node_mapping(SortPath);
node_mapping(IncrementalSortPath);
node_mapping(GroupPath);
#if PG_MAJORVERSION_NUM < 19
node_mapping(UpperUniquePath);
#endif
node_mapping(AggPath);
node_mapping(GroupingSetData);
node_mapping(RollupData);
node_mapping(GroupingSetsPath);
node_mapping(MinMaxAggPath);
node_mapping(WindowAggPath);
node_mapping(SetOpPath);
node_mapping(RecursiveUnionPath);
node_mapping(LockRowsPath);
node_mapping(ModifyTablePath);
node_mapping(LimitPath);
node_mapping(RestrictInfo);
node_mapping(PlaceHolderVar);
node_mapping(SpecialJoinInfo);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(OuterJoinClauseInfo);
#endif
node_mapping(AppendRelInfo);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(RowIdentityVarInfo);
#endif
node_mapping(PlaceHolderInfo);
node_mapping(MinMaxAggInfo);
node_mapping(PlannerParamItem);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(AggInfo);
node_mapping(AggTransInfo);
#endif
#if PG_MAJORVERSION_NUM >= 18
node_mapping(UniqueRelInfo);
#endif
node_mapping(PlannedStmt);
node_mapping(Result);
node_mapping(ProjectSet);
node_mapping(ModifyTable);
node_mapping(Append);
node_mapping(MergeAppend);
node_mapping(RecursiveUnion);
node_mapping(BitmapAnd);
node_mapping(BitmapOr);
node_mapping(SeqScan);
node_mapping(SampleScan);
node_mapping(IndexScan);
node_mapping(IndexOnlyScan);
node_mapping(BitmapIndexScan);
node_mapping(BitmapHeapScan);
node_mapping(TidScan);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(TidRangeScan);
#endif
node_mapping(SubqueryScan);
node_mapping(FunctionScan);
node_mapping(ValuesScan);
node_mapping(TableFuncScan);
node_mapping(CteScan);
node_mapping(NamedTuplestoreScan);
node_mapping(WorkTableScan);
node_mapping(ForeignScan);
node_mapping(CustomScan);
node_mapping(NestLoop);
node_mapping(NestLoopParam);
node_mapping(MergeJoin);
node_mapping(HashJoin);
node_mapping(Material);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(Memoize);
#endif
node_mapping(Sort);
node_mapping(IncrementalSort);
node_mapping(Group);
node_mapping(Agg);
node_mapping(WindowAgg);
node_mapping(Unique);
node_mapping(Gather);
node_mapping(GatherMerge);
node_mapping(Hash);
node_mapping(SetOp);
node_mapping(LockRows);
node_mapping(Limit);
node_mapping(PlanRowMark);
node_mapping(PartitionPruneInfo);
node_mapping(PartitionedRelPruneInfo);
node_mapping(PartitionPruneStepOp);
node_mapping(PartitionPruneStepCombine);
node_mapping(PlanInvalItem);
node_mapping(ExprState);
node_mapping(IndexInfo);
node_mapping(ExprContext);
node_mapping(ReturnSetInfo);
node_mapping(ProjectionInfo);
node_mapping(JunkFilter);
node_mapping(OnConflictSetState);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(MergeActionState);
#endif
node_mapping(ResultRelInfo);
node_mapping(EState);
node_mapping(WindowFuncExprState);
node_mapping(SetExprState);
node_mapping(SubPlanState);
node_mapping(DomainConstraintState);
node_mapping(ResultState);
node_mapping(ProjectSetState);
node_mapping(ModifyTableState);
node_mapping(AppendState);
node_mapping(MergeAppendState);
node_mapping(RecursiveUnionState);
node_mapping(BitmapAndState);
node_mapping(BitmapOrState);
node_mapping(ScanState);
node_mapping(SeqScanState);
node_mapping(SampleScanState);
node_mapping(IndexScanState);
node_mapping(IndexOnlyScanState);
node_mapping(BitmapIndexScanState);
node_mapping(BitmapHeapScanState);
node_mapping(TidScanState);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(TidRangeScanState);
#endif
node_mapping(SubqueryScanState);
node_mapping(FunctionScanState);
node_mapping(ValuesScanState);
node_mapping(TableFuncScanState);
node_mapping(CteScanState);
node_mapping(NamedTuplestoreScanState);
node_mapping(WorkTableScanState);
node_mapping(ForeignScanState);
node_mapping(CustomScanState);
node_mapping(JoinState);
node_mapping(NestLoopState);
node_mapping(MergeJoinState);
node_mapping(HashJoinState);
node_mapping(MaterialState);
#if PG_MAJORVERSION_NUM >= 14
node_mapping(MemoizeState);
#endif
node_mapping(SortState);
node_mapping(IncrementalSortState);
node_mapping(GroupState);
node_mapping(AggState);
node_mapping(WindowAggState);
node_mapping(UniqueState);
node_mapping(GatherState);
node_mapping(GatherMergeState);
node_mapping(HashState);
node_mapping(SetOpState);
node_mapping(LockRowsState);
node_mapping(LimitState);
node_mapping(IndexAmRoutine);
node_mapping(TableAmRoutine);
node_mapping(TsmRoutine);
node_mapping(EventTriggerData);
node_mapping(TriggerData);
node_mapping(TupleTableSlot);
node_mapping(FdwRoutine);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(Bitmapset);
#endif
node_mapping(ExtensibleNode);
#if PG_MAJORVERSION_NUM >= 16
node_mapping(ErrorSaveContext);
#endif
node_mapping(IdentifySystemCmd);
node_mapping(BaseBackupCmd);
node_mapping(CreateReplicationSlotCmd);
node_mapping(DropReplicationSlotCmd);
#if PG_MAJORVERSION_NUM >= 17
node_mapping(AlterReplicationSlotCmd);
#endif
node_mapping(StartReplicationCmd);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(ReadReplicationSlotCmd);
#endif
node_mapping(TimeLineHistoryCmd);
#if PG_MAJORVERSION_NUM >= 17
node_mapping(UploadManifestCmd);
#endif
node_mapping(SupportRequestSimplify);
node_mapping(SupportRequestSelectivity);
node_mapping(SupportRequestCost);
node_mapping(SupportRequestRows);
node_mapping(SupportRequestIndexCondition);
#if PG_MAJORVERSION_NUM >= 15
node_mapping(SupportRequestWFuncMonotonic);
#endif
#if PG_MAJORVERSION_NUM >= 17
node_mapping(SupportRequestOptimizeWindowClause);
#endif
#if PG_MAJORVERSION_NUM >= 18
node_mapping(SupportRequestModifyInPlace);
#endif
#if PG_MAJORVERSION_NUM >= 15
node_mapping(Integer);
node_mapping(Float);
node_mapping(Boolean);
node_mapping(String);
node_mapping(BitString);
#endif
node_mapping(ForeignKeyCacheInfo);

#undef node_mapping

namespace nodes {
template <typename T> struct unknown_node {
  T node;
};
}; // namespace nodes

#define node_dispatch(name)                                                                        \
  case T_##name:                                                                                   \
    static_assert(                                                                                 \
        covering_node<std::remove_pointer_t<decltype(reinterpret_cast<nodes::name *>(node))>>);    \
    visitor(*reinterpret_cast<nodes::name *>(node));                                               \
    break

template <typename T>
concept covering_node =
    std::is_class_v<node_traits<T>> && requires { typename T::underlying_type; };

template <typename Visitor> void visit_node(covering_node auto node, Visitor &&visitor) {
  visitor(node);
}

template <typename T>
concept covered_node = std::is_class_v<node_coverage<T>> &&
                       requires { typename node_coverage<T>::type::underlying_type; };

template <typename Visitor> void visit_node(covered_node auto *node, Visitor &&visitor) {
  visitor(*reinterpret_cast<node_coverage<std::remove_pointer_t<decltype(node)>>::type *>(node));
}

template <typename Visitor> void visit_node(void *node, Visitor &&visitor) {
  switch (nodeTag(node)) {
    // clang-format off
node_dispatch(List);
node_dispatch(Alias);
node_dispatch(RangeVar);
node_dispatch(TableFunc);
node_dispatch(IntoClause);
node_dispatch(Var);
node_dispatch(Const);
node_dispatch(Param);
node_dispatch(Aggref);
node_dispatch(GroupingFunc);
node_dispatch(WindowFunc);
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(WindowFuncRunCondition);
node_dispatch(MergeSupportFunc);
#endif
node_dispatch(SubscriptingRef);
node_dispatch(FuncExpr);
node_dispatch(NamedArgExpr);
node_dispatch(OpExpr);
node_dispatch(DistinctExpr);
node_dispatch(NullIfExpr);
node_dispatch(ScalarArrayOpExpr);
node_dispatch(BoolExpr);
node_dispatch(SubLink);
node_dispatch(SubPlan);
node_dispatch(AlternativeSubPlan);
node_dispatch(FieldSelect);
node_dispatch(FieldStore);
node_dispatch(RelabelType);
node_dispatch(CoerceViaIO);
node_dispatch(ArrayCoerceExpr);
node_dispatch(ConvertRowtypeExpr);
node_dispatch(CollateExpr);
node_dispatch(CaseExpr);
node_dispatch(CaseWhen);
node_dispatch(CaseTestExpr);
node_dispatch(ArrayExpr);
node_dispatch(RowExpr);
node_dispatch(RowCompareExpr);
node_dispatch(CoalesceExpr);
node_dispatch(MinMaxExpr);
node_dispatch(SQLValueFunction);
node_dispatch(XmlExpr);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(JsonFormat);
node_dispatch(JsonReturning);
node_dispatch(JsonValueExpr);
node_dispatch(JsonConstructorExpr);
node_dispatch(JsonIsPredicate);
#endif
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(JsonBehavior);
node_dispatch(JsonExpr);
node_dispatch(JsonTablePath);
node_dispatch(JsonTablePathScan);
node_dispatch(JsonTableSiblingJoin);
#endif
node_dispatch(NullTest);
node_dispatch(BooleanTest);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(MergeAction);
#endif
node_dispatch(CoerceToDomain);
node_dispatch(CoerceToDomainValue);
node_dispatch(SetToDefault);
node_dispatch(CurrentOfExpr);
node_dispatch(NextValueExpr);
node_dispatch(InferenceElem);
#if PG_MAJORVERSION_NUM >= 18
node_dispatch(ReturningExpr);
#endif
node_dispatch(TargetEntry);
node_dispatch(RangeTblRef);
node_dispatch(JoinExpr);
node_dispatch(FromExpr);
node_dispatch(OnConflictExpr);
node_dispatch(Query);
node_dispatch(TypeName);
node_dispatch(ColumnRef);
node_dispatch(ParamRef);
node_dispatch(A_Expr);
node_dispatch(A_Const);
node_dispatch(TypeCast);
node_dispatch(CollateClause);
node_dispatch(RoleSpec);
node_dispatch(FuncCall);
node_dispatch(A_Star);
node_dispatch(A_Indices);
node_dispatch(A_Indirection);
node_dispatch(A_ArrayExpr);
node_dispatch(ResTarget);
node_dispatch(MultiAssignRef);
node_dispatch(SortBy);
node_dispatch(WindowDef);
node_dispatch(RangeSubselect);
node_dispatch(RangeFunction);
node_dispatch(RangeTableFunc);
node_dispatch(RangeTableFuncCol);
node_dispatch(RangeTableSample);
node_dispatch(ColumnDef);
node_dispatch(TableLikeClause);
node_dispatch(IndexElem);
node_dispatch(DefElem);
node_dispatch(LockingClause);
node_dispatch(XmlSerialize);
node_dispatch(PartitionElem);
#if PG_MAJORVERSION_NUM == 17
node_dispatch(SinglePartitionSpec);
#endif
node_dispatch(PartitionSpec);
node_dispatch(PartitionBoundSpec);
node_dispatch(PartitionRangeDatum);
node_dispatch(PartitionCmd);
node_dispatch(RangeTblEntry);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(RTEPermissionInfo);
#endif
node_dispatch(RangeTblFunction);
node_dispatch(TableSampleClause);
node_dispatch(WithCheckOption);
node_dispatch(SortGroupClause);
node_dispatch(GroupingSet);
node_dispatch(WindowClause);
node_dispatch(RowMarkClause);
node_dispatch(WithClause);
node_dispatch(InferClause);
node_dispatch(OnConflictClause);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(CTESearchClause);
node_dispatch(CTECycleClause);
#endif
node_dispatch(CommonTableExpr);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(MergeWhenClause);
#endif
#if PG_MAJORVERSION_NUM >= 18
node_dispatch(ReturningOption);
node_dispatch(ReturningClause);
#endif
node_dispatch(TriggerTransition);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(JsonOutput);
#endif
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(JsonArgument);
node_dispatch(JsonFuncExpr);
node_dispatch(JsonTablePathSpec);
node_dispatch(JsonTable);
node_dispatch(JsonTableColumn);
node_dispatch(JsonKeyValue);
node_dispatch(JsonParseExpr);
node_dispatch(JsonScalarExpr);
node_dispatch(JsonSerializeExpr);
#endif
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(JsonObjectConstructor);
node_dispatch(JsonArrayConstructor);
node_dispatch(JsonArrayQueryConstructor);
node_dispatch(JsonAggConstructor);
node_dispatch(JsonObjectAgg);
node_dispatch(JsonArrayAgg);
#endif
node_dispatch(RawStmt);
node_dispatch(InsertStmt);
node_dispatch(DeleteStmt);
node_dispatch(UpdateStmt);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(MergeStmt);
#endif
node_dispatch(SelectStmt);
node_dispatch(SetOperationStmt);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(ReturnStmt);
#endif
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(PLAssignStmt);
#endif
node_dispatch(CreateSchemaStmt);
node_dispatch(AlterTableStmt);
node_dispatch(AlterTableCmd);
#if PG_MAJORVERSION_NUM >= 18
node_dispatch(ATAlterConstraint);
#endif
node_dispatch(ReplicaIdentityStmt);
node_dispatch(AlterCollationStmt);
node_dispatch(AlterDomainStmt);
node_dispatch(GrantStmt);
node_dispatch(ObjectWithArgs);
node_dispatch(AccessPriv);
node_dispatch(GrantRoleStmt);
node_dispatch(AlterDefaultPrivilegesStmt);
node_dispatch(CopyStmt);
node_dispatch(VariableSetStmt);
node_dispatch(VariableShowStmt);
node_dispatch(CreateStmt);
node_dispatch(Constraint);
node_dispatch(CreateTableSpaceStmt);
node_dispatch(DropTableSpaceStmt);
node_dispatch(AlterTableSpaceOptionsStmt);
node_dispatch(AlterTableMoveAllStmt);
node_dispatch(CreateExtensionStmt);
node_dispatch(AlterExtensionStmt);
node_dispatch(AlterExtensionContentsStmt);
node_dispatch(CreateFdwStmt);
node_dispatch(AlterFdwStmt);
node_dispatch(CreateForeignServerStmt);
node_dispatch(AlterForeignServerStmt);
node_dispatch(CreateForeignTableStmt);
node_dispatch(CreateUserMappingStmt);
node_dispatch(AlterUserMappingStmt);
node_dispatch(DropUserMappingStmt);
node_dispatch(ImportForeignSchemaStmt);
node_dispatch(CreatePolicyStmt);
node_dispatch(AlterPolicyStmt);
node_dispatch(CreateAmStmt);
node_dispatch(CreateTrigStmt);
node_dispatch(CreateEventTrigStmt);
node_dispatch(AlterEventTrigStmt);
node_dispatch(CreatePLangStmt);
node_dispatch(CreateRoleStmt);
node_dispatch(AlterRoleStmt);
node_dispatch(AlterRoleSetStmt);
node_dispatch(DropRoleStmt);
node_dispatch(CreateSeqStmt);
node_dispatch(AlterSeqStmt);
node_dispatch(DefineStmt);
node_dispatch(CreateDomainStmt);
node_dispatch(CreateOpClassStmt);
node_dispatch(CreateOpClassItem);
node_dispatch(CreateOpFamilyStmt);
node_dispatch(AlterOpFamilyStmt);
node_dispatch(DropStmt);
node_dispatch(TruncateStmt);
node_dispatch(CommentStmt);
node_dispatch(SecLabelStmt);
node_dispatch(DeclareCursorStmt);
node_dispatch(ClosePortalStmt);
node_dispatch(FetchStmt);
node_dispatch(IndexStmt);
node_dispatch(CreateStatsStmt);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(StatsElem);
#endif
node_dispatch(AlterStatsStmt);
node_dispatch(CreateFunctionStmt);
node_dispatch(FunctionParameter);
node_dispatch(AlterFunctionStmt);
node_dispatch(DoStmt);
node_dispatch(InlineCodeBlock);
node_dispatch(CallStmt);
node_dispatch(CallContext);
node_dispatch(RenameStmt);
node_dispatch(AlterObjectDependsStmt);
node_dispatch(AlterObjectSchemaStmt);
node_dispatch(AlterOwnerStmt);
node_dispatch(AlterOperatorStmt);
node_dispatch(AlterTypeStmt);
node_dispatch(RuleStmt);
node_dispatch(NotifyStmt);
node_dispatch(ListenStmt);
node_dispatch(UnlistenStmt);
node_dispatch(TransactionStmt);
node_dispatch(CompositeTypeStmt);
node_dispatch(CreateEnumStmt);
node_dispatch(CreateRangeStmt);
node_dispatch(AlterEnumStmt);
node_dispatch(ViewStmt);
node_dispatch(LoadStmt);
node_dispatch(CreatedbStmt);
node_dispatch(AlterDatabaseStmt);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(AlterDatabaseRefreshCollStmt);
#endif
node_dispatch(AlterDatabaseSetStmt);
node_dispatch(DropdbStmt);
node_dispatch(AlterSystemStmt);
node_dispatch(ClusterStmt);
node_dispatch(VacuumStmt);
node_dispatch(VacuumRelation);
node_dispatch(ExplainStmt);
node_dispatch(CreateTableAsStmt);
node_dispatch(RefreshMatViewStmt);
node_dispatch(CheckPointStmt);
node_dispatch(DiscardStmt);
node_dispatch(LockStmt);
node_dispatch(ConstraintsSetStmt);
node_dispatch(ReindexStmt);
node_dispatch(CreateConversionStmt);
node_dispatch(CreateCastStmt);
node_dispatch(CreateTransformStmt);
node_dispatch(PrepareStmt);
node_dispatch(ExecuteStmt);
node_dispatch(DeallocateStmt);
node_dispatch(DropOwnedStmt);
node_dispatch(ReassignOwnedStmt);
node_dispatch(AlterTSDictionaryStmt);
node_dispatch(AlterTSConfigurationStmt);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(PublicationTable);
node_dispatch(PublicationObjSpec);
#endif
node_dispatch(CreatePublicationStmt);
node_dispatch(AlterPublicationStmt);
node_dispatch(CreateSubscriptionStmt);
node_dispatch(AlterSubscriptionStmt);
node_dispatch(DropSubscriptionStmt);
node_dispatch(PlannerGlobal);
node_dispatch(PlannerInfo);
node_dispatch(RelOptInfo);
node_dispatch(IndexOptInfo);
node_dispatch(ForeignKeyOptInfo);
node_dispatch(StatisticExtInfo);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(JoinDomain);
#endif
node_dispatch(EquivalenceClass);
node_dispatch(EquivalenceMember);
node_dispatch(PathKey);
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(GroupByOrdering);
#endif
node_dispatch(PathTarget);
node_dispatch(ParamPathInfo);
node_dispatch(Path);
node_dispatch(IndexPath);
node_dispatch(IndexClause);
node_dispatch(BitmapHeapPath);
node_dispatch(BitmapAndPath);
node_dispatch(BitmapOrPath);
node_dispatch(TidPath);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(TidRangePath);
#endif
node_dispatch(SubqueryScanPath);
node_dispatch(ForeignPath);
node_dispatch(CustomPath);
node_dispatch(AppendPath);
node_dispatch(MergeAppendPath);
node_dispatch(GroupResultPath);
node_dispatch(MaterialPath);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(MemoizePath);
#endif
node_dispatch(UniquePath);
node_dispatch(GatherPath);
node_dispatch(GatherMergePath);
node_dispatch(NestPath);
node_dispatch(MergePath);
node_dispatch(HashPath);
node_dispatch(ProjectionPath);
node_dispatch(ProjectSetPath);
node_dispatch(SortPath);
node_dispatch(IncrementalSortPath);
node_dispatch(GroupPath);
#if PG_MAJORVERSION_NUM < 19
node_dispatch(UpperUniquePath);
#endif
node_dispatch(AggPath);
node_dispatch(GroupingSetData);
node_dispatch(RollupData);
node_dispatch(GroupingSetsPath);
node_dispatch(MinMaxAggPath);
node_dispatch(WindowAggPath);
node_dispatch(SetOpPath);
node_dispatch(RecursiveUnionPath);
node_dispatch(LockRowsPath);
node_dispatch(ModifyTablePath);
node_dispatch(LimitPath);
node_dispatch(RestrictInfo);
node_dispatch(PlaceHolderVar);
node_dispatch(SpecialJoinInfo);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(OuterJoinClauseInfo);
#endif
node_dispatch(AppendRelInfo);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(RowIdentityVarInfo);
#endif
node_dispatch(PlaceHolderInfo);
node_dispatch(MinMaxAggInfo);
node_dispatch(PlannerParamItem);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(AggInfo);
node_dispatch(AggTransInfo);
#endif
#if PG_MAJORVERSION_NUM >= 18
node_dispatch(UniqueRelInfo);
#endif
node_dispatch(PlannedStmt);
node_dispatch(Result);
node_dispatch(ProjectSet);
node_dispatch(ModifyTable);
node_dispatch(Append);
node_dispatch(MergeAppend);
node_dispatch(RecursiveUnion);
node_dispatch(BitmapAnd);
node_dispatch(BitmapOr);
node_dispatch(SeqScan);
node_dispatch(SampleScan);
node_dispatch(IndexScan);
node_dispatch(IndexOnlyScan);
node_dispatch(BitmapIndexScan);
node_dispatch(BitmapHeapScan);
node_dispatch(TidScan);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(TidRangeScan);
#endif
node_dispatch(SubqueryScan);
node_dispatch(FunctionScan);
node_dispatch(ValuesScan);
node_dispatch(TableFuncScan);
node_dispatch(CteScan);
node_dispatch(NamedTuplestoreScan);
node_dispatch(WorkTableScan);
node_dispatch(ForeignScan);
node_dispatch(CustomScan);
node_dispatch(NestLoop);
node_dispatch(NestLoopParam);
node_dispatch(MergeJoin);
node_dispatch(HashJoin);
node_dispatch(Material);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(Memoize);
#endif
node_dispatch(Sort);
node_dispatch(IncrementalSort);
node_dispatch(Group);
node_dispatch(Agg);
node_dispatch(WindowAgg);
node_dispatch(Unique);
node_dispatch(Gather);
node_dispatch(GatherMerge);
node_dispatch(Hash);
node_dispatch(SetOp);
node_dispatch(LockRows);
node_dispatch(Limit);
node_dispatch(PlanRowMark);
node_dispatch(PartitionPruneInfo);
node_dispatch(PartitionedRelPruneInfo);
node_dispatch(PartitionPruneStepOp);
node_dispatch(PartitionPruneStepCombine);
node_dispatch(PlanInvalItem);
node_dispatch(ExprState);
node_dispatch(IndexInfo);
node_dispatch(ExprContext);
node_dispatch(ReturnSetInfo);
node_dispatch(ProjectionInfo);
node_dispatch(JunkFilter);
node_dispatch(OnConflictSetState);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(MergeActionState);
#endif
node_dispatch(ResultRelInfo);
node_dispatch(EState);
node_dispatch(WindowFuncExprState);
node_dispatch(SetExprState);
node_dispatch(SubPlanState);
node_dispatch(DomainConstraintState);
node_dispatch(ResultState);
node_dispatch(ProjectSetState);
node_dispatch(ModifyTableState);
node_dispatch(AppendState);
node_dispatch(MergeAppendState);
node_dispatch(RecursiveUnionState);
node_dispatch(BitmapAndState);
node_dispatch(BitmapOrState);
node_dispatch(ScanState);
node_dispatch(SeqScanState);
node_dispatch(SampleScanState);
node_dispatch(IndexScanState);
node_dispatch(IndexOnlyScanState);
node_dispatch(BitmapIndexScanState);
node_dispatch(BitmapHeapScanState);
node_dispatch(TidScanState);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(TidRangeScanState);
#endif
node_dispatch(SubqueryScanState);
node_dispatch(FunctionScanState);
node_dispatch(ValuesScanState);
node_dispatch(TableFuncScanState);
node_dispatch(CteScanState);
node_dispatch(NamedTuplestoreScanState);
node_dispatch(WorkTableScanState);
node_dispatch(ForeignScanState);
node_dispatch(CustomScanState);
node_dispatch(JoinState);
node_dispatch(NestLoopState);
node_dispatch(MergeJoinState);
node_dispatch(HashJoinState);
node_dispatch(MaterialState);
#if PG_MAJORVERSION_NUM >= 14
node_dispatch(MemoizeState);
#endif
node_dispatch(SortState);
node_dispatch(IncrementalSortState);
node_dispatch(GroupState);
node_dispatch(AggState);
node_dispatch(WindowAggState);
node_dispatch(UniqueState);
node_dispatch(GatherState);
node_dispatch(GatherMergeState);
node_dispatch(HashState);
node_dispatch(SetOpState);
node_dispatch(LockRowsState);
node_dispatch(LimitState);
node_dispatch(IndexAmRoutine);
node_dispatch(TableAmRoutine);
node_dispatch(TsmRoutine);
node_dispatch(EventTriggerData);
node_dispatch(TriggerData);
node_dispatch(TupleTableSlot);
node_dispatch(FdwRoutine);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(Bitmapset);
#endif
node_dispatch(ExtensibleNode);
#if PG_MAJORVERSION_NUM >= 16
node_dispatch(ErrorSaveContext);
#endif
node_dispatch(IdentifySystemCmd);
node_dispatch(BaseBackupCmd);
node_dispatch(CreateReplicationSlotCmd);
node_dispatch(DropReplicationSlotCmd);
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(AlterReplicationSlotCmd);
#endif
node_dispatch(StartReplicationCmd);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(ReadReplicationSlotCmd);
#endif
node_dispatch(TimeLineHistoryCmd);
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(UploadManifestCmd);
#endif
node_dispatch(SupportRequestSimplify);
node_dispatch(SupportRequestSelectivity);
node_dispatch(SupportRequestCost);
node_dispatch(SupportRequestRows);
node_dispatch(SupportRequestIndexCondition);
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(SupportRequestWFuncMonotonic);
#endif
#if PG_MAJORVERSION_NUM >= 17
node_dispatch(SupportRequestOptimizeWindowClause);
#endif
#if PG_MAJORVERSION_NUM >= 18
node_dispatch(SupportRequestModifyInPlace);
#endif
#if PG_MAJORVERSION_NUM >= 15
node_dispatch(Integer);
node_dispatch(Float);
node_dispatch(Boolean);
node_dispatch(String);
node_dispatch(BitString);
#endif
node_dispatch(ForeignKeyCacheInfo);
default:
break;
}
  // clang-format on
  if constexpr (requires {
                  std::declval<Visitor>()(std::declval<nodes::unknown_node<decltype(node)>>());
                }) {
    visitor(nodes::unknown_node<decltype(node)>{node});
  }
  else {
    throw std::runtime_error("unknown node tag");
  }
}

template <typename T>
concept walker_implementation = requires(T t, ::Node *node, ::tree_walker_callback cb, void *ctx) {
  { t(node, cb, ctx) } -> std::same_as<bool>;
};

template <typename T> struct node_walker {
  void operator()(T &node, auto &&visitor, const walker_implementation auto &walker)
      requires covering_node<T>
  {
    auto *recasted_node = reinterpret_cast<::Node *>(node.as_ptr());
    (*this)(recasted_node, visitor, walker);
  }

  void operator()(::Node *recasted_node, auto &&visitor, const walker_implementation auto &walker) {
    /// In theory, this should have worked, but in practice some structures are
    /// way too large. Also, makes the binary really large. TODO: wait for C++26/P3435?
    //
    //    typename T::underlying_type &t = *node;
    //    boost::pfr::for_each_field(t, [visitor](const auto &field, const auto index) {
    //      if constexpr (std::is_pointer_v<std::remove_cvref_t<decltype(visitor)>>) {
    //        cppgres::visit_node(field, *visitor);
    //      } else {
    //        cppgres::visit_node(field, visitor);
    //      }
    //    });


      struct _ctx {
        decltype(visitor) _visitor;
      };
      _ctx c{std::forward<decltype(visitor)>(visitor)};
      ffi_guard{walker}(
          recasted_node,
          [](::Node *node, void *ctx) {
            if (node == nullptr) {
              return false;
            }
            _ctx *c = reinterpret_cast<_ctx *>(ctx);
            if constexpr (std::is_pointer_v<std::remove_cvref_t<decltype(visitor)>>) {
              cppgres::visit_node(node, *c->_visitor);
            } else {
              cppgres::visit_node(node, c->_visitor);
            }
            return false;
          },
          &c);
    }
};

template <> struct node_walker<nodes::RawStmt> {
  void operator()(nodes::RawStmt &node, auto &&visitor, const walker_implementation auto &walker) {
    if constexpr (std::is_pointer_v<std::remove_cvref_t<decltype(visitor)>>) {
      cppgres::visit_node(node.as_ref().stmt, *visitor);
    } else {
      cppgres::visit_node(node.as_ref().stmt, visitor);
    }
  }
};

template <> struct node_walker<nodes::List> {
  void operator()(nodes::List &node, auto &&visitor, const walker_implementation auto &walker) {
    ::ListCell *lc;
    ::List *node_p = node.as_ptr();
    if (nodeTag(node_p) == T_List) {
      foreach (lc, node_p) {
        void *node = lfirst(lc);
        if constexpr (std::is_pointer_v<std::remove_cvref_t<decltype(visitor)>>) {
          cppgres::visit_node(node, *visitor);
        } else {
          cppgres::visit_node(node, visitor);
        }
      }
    }
  }
};

template <covering_node T> struct raw_expr_node_walker {
  void operator()(T &node, auto &&visitor) {
    _walker(node, std::forward<decltype(visitor)>(visitor),
#if PG_MAJORVERSION_NUM < 16
            reinterpret_cast<bool (*)(::Node *, ::tree_walker_callback, void *)>(
                raw_expression_tree_walker)
#else
            ::raw_expression_tree_walker_impl
#endif
    );
  }

private:
  node_walker<T> _walker;
};

template <typename T> struct expr_node_walker {

  void operator()(T &node, auto &&visitor) {
    _walker(node, std::forward<decltype(visitor)>(visitor),
#if PG_MAJORVERSION_NUM < 16
            reinterpret_cast<bool (*)(::Node *, ::tree_walker_callback, void *)>(
                ::expression_tree_walker)
#else
            ::expression_tree_walker_impl
#endif
    );
  }

private:
  node_walker<T> _walker;
};

template <> struct expr_node_walker<nodes::Query> {
  expr_node_walker() {}
  explicit expr_node_walker(int flags) : _flags(flags) {}
  void operator()(nodes::Query &node, auto &&visitor) {
    _walker(node, std::forward<decltype(visitor)>(visitor),
            [this](auto query, auto cb, auto ctx) -> bool {
              return
#if PG_MAJORVERSION_NUM < 16
                  reinterpret_cast<bool (*)(::Query *, ::tree_walker_callback, void *, int)>(
                      ::query_tree_walker)(reinterpret_cast<::Query *>(query), cb, ctx, _flags);
#else
                  ::query_tree_walker_impl
                  (reinterpret_cast<::Query *>(query), cb, ctx, _flags);
#endif
            });
  }

private:
  node_walker<nodes::Query> _walker;
  int _flags = QTW_EXAMINE_SORTGROUP | QTW_EXAMINE_RTES_BEFORE;
};

#undef node_dispatch

} // namespace cppgres
