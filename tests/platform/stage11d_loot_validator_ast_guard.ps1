param(
    [Parameter(Mandatory = $true)]
    [string]$ValidatorPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

function Normalized([string]$Text) {
    return ($Text -replace '[ \t\r\n]+', '')
}

function TopLevelAssignment($ScriptAst, [string]$VariableName, [string]$Reason) {
    $assignments = @($ScriptAst.EndBlock.Statements | Where-Object {
        $_ -is [System.Management.Automation.Language.AssignmentStatementAst] -and
        $_.Left -is [System.Management.Automation.Language.VariableExpressionAst] -and
        $_.Left.VariablePath.UserPath -eq $VariableName
    })
    if ($assignments.Count -ne 1) {
        Fail $Reason
    }
    $assignment = $assignments[0]
    if ($assignment.Operator -ne
        [System.Management.Automation.Language.TokenKind]::Equals) {
        Fail $Reason
    }
    if ($assignment.Right -is [System.Management.Automation.Language.StringConstantExpressionAst] -or
        $assignment.Right -is [System.Management.Automation.Language.ExpandableStringExpressionAst]) {
        Fail $Reason
    }
    return $assignment
}

function TopLevelRequire($ScriptAst, [string]$NormalizedCommand, [string]$Reason) {
    $commands = @($ScriptAst.EndBlock.Statements | ForEach-Object {
        if ($_ -is [System.Management.Automation.Language.PipelineAst] -and
            $_.PipelineElements.Count -eq 1 -and
            $_.PipelineElements[0] -is [System.Management.Automation.Language.CommandAst] -and
            $_.PipelineElements[0].GetCommandName() -eq 'Require' -and
            (Normalized $_.Extent.Text) -eq $NormalizedCommand) {
            $_
        }
    })
    if ($commands.Count -ne 1) {
        Fail $Reason
    }
    return $commands[0]
}

function RequireImmediatelyAfter($ScriptAst, $Assignment, $Require, [string]$Reason) {
    $statements = @($ScriptAst.EndBlock.Statements)
    $assignmentIndex = [array]::IndexOf($statements, $Assignment)
    $requireIndex = [array]::IndexOf($statements, $Require)
    if ($assignmentIndex -lt 0 -or $requireIndex -ne ($assignmentIndex + 1)) {
        Fail $Reason
    }
}

function RequireNoStringExpression($Ast, [string]$Reason) {
    $strings = @($Ast.FindAll({
        param($node)
        ($node -is [System.Management.Automation.Language.StringConstantExpressionAst] -and
            $node.StringConstantType -ne
                [System.Management.Automation.Language.StringConstantType]::BareWord) -or
        $node -is [System.Management.Automation.Language.ExpandableStringExpressionAst]
    }, $true))
    if ($strings.Count -ne 0) {
        Fail $Reason
    }
}

function OptionalAstTruthy($Node, [string]$Name) {
    [object[]]$properties = @($Node.PSObject.Properties | Where-Object {
        $_.Name -eq $Name
    })
    return $properties.Length -eq 1 -and [bool]($properties[0].Value)
}

function DirectArrayPipeline($Assignment, [string]$Reason) {
    if ($Assignment.Right -isnot
        [System.Management.Automation.Language.CommandExpressionAst] -or
        $Assignment.Right.Expression -isnot
            [System.Management.Automation.Language.ArrayExpressionAst]) {
        Fail $Reason
    }
    $statements = @($Assignment.Right.Expression.SubExpression.Statements)
    if ($statements.Count -ne 1 -or
        $statements[0] -isnot [System.Management.Automation.Language.PipelineAst]) {
        Fail $Reason
    }
    if (OptionalAstTruthy $statements[0] 'Background') {
        Fail $Reason
    }
    return $statements[0]
}

function MemberBinding($Node) {
    if ($Node -isnot [System.Management.Automation.Language.MemberExpressionAst] -or
        $Node.Static -or
        (OptionalAstTruthy $Node 'NullConditional') -or
        $Node.Expression -isnot
            [System.Management.Automation.Language.VariableExpressionAst] -or
        $Node.Expression.Splatted -or
        $Node.Member -isnot
            [System.Management.Automation.Language.StringConstantExpressionAst] -or
        $Node.Member.StringConstantType -ne
            [System.Management.Automation.Language.StringConstantType]::BareWord) {
        return $null
    }
    return ('${0}.{1}' -f $Node.Expression.VariablePath.UserPath,
        $Node.Member.Value)
}

function TupleComparison($Element, [string]$Reason) {
    if ($Element -isnot [System.Management.Automation.Language.ParenExpressionAst] -or
        $Element.Pipeline.PipelineElements.Count -ne 1 -or
        $Element.Pipeline.PipelineElements[0] -isnot
            [System.Management.Automation.Language.CommandExpressionAst]) {
        Fail $Reason
    }
    $comparison = $Element.Pipeline.PipelineElements[0].Expression
    if ($comparison -isnot [System.Management.Automation.Language.BinaryExpressionAst] -or
        $comparison.Operator -ne
            [System.Management.Automation.Language.TokenKind]::Ieq -or
        $comparison.Left -isnot [System.Management.Automation.Language.MemberExpressionAst] -or
        $comparison.Right -isnot [System.Management.Automation.Language.MemberExpressionAst]) {
        Fail $Reason
    }
    return $comparison
}

if (-not (Test-Path -LiteralPath $ValidatorPath -PathType Leaf)) {
    Fail 'Stage11D loot evidence guard AST validator input is missing'
}

$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path -LiteralPath $ValidatorPath), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    Fail 'Stage11D loot evidence guard validator AST parse failed'
}

$tupleAssignmentReason =
    'Stage11D loot evidence guard requires real top-level per-rarity tuple assignment'
$tupleAssignment = TopLevelAssignment $ast 'ordinaryTupleChecks' $tupleAssignmentReason
$tuplePipeline = DirectArrayPipeline $tupleAssignment $tupleAssignmentReason
if ($tuplePipeline.PipelineElements.Count -ne 1 -or
    $tuplePipeline.PipelineElements[0] -isnot
        [System.Management.Automation.Language.CommandExpressionAst] -or
    $tuplePipeline.PipelineElements[0].Expression -isnot
        [System.Management.Automation.Language.ArrayLiteralAst]) {
    Fail $tupleAssignmentReason
}
$tupleElements = @($tuplePipeline.PipelineElements[0].Expression.Elements)
if ($tupleElements.Count -ne 15) {
    Fail $tupleAssignmentReason
}
$expectedTupleBindings = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal)
foreach ($scenario in 'show', 'magic', 'rare', 'preview', 'pickup') {
    foreach ($rarity in 'normal', 'magic', 'rare') {
        [void]$expectedTupleBindings.Add(
            ('${0}.observed_{1}|$manifest.ordinary_{1}_tuple' -f
                $scenario, $rarity))
    }
}
$actualTupleBindings = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal)
foreach ($element in $tupleElements) {
    $comparison = TupleComparison $element $tupleAssignmentReason
    $binding = '{0}|{1}' -f (MemberBinding $comparison.Left),
        (MemberBinding $comparison.Right)
    if (-not $expectedTupleBindings.Contains($binding) -or
        -not $actualTupleBindings.Add($binding)) {
        Fail $tupleAssignmentReason
    }
}
if ($actualTupleBindings.Count -ne $expectedTupleBindings.Count) {
    Fail $tupleAssignmentReason
}

$tupleRequire = TopLevelRequire $ast "Require(-not(`$ordinaryTupleChecks-contains`$false))'productionraritytuplesdrifted'" 'Stage11D loot evidence guard requires exact per-rarity tuples in a top-level executable Require command'
RequireImmediatelyAfter $ast $tupleAssignment $tupleRequire 'Stage11D loot evidence guard requires per-rarity tuple assignment immediately before its Require'

$pickupDerivationReason =
    'Stage11D loot evidence guard requires real top-level pickup conservation derivation'
$pickupAssignment = TopLevelAssignment $ast 'expectedPickupSnapshot' $pickupDerivationReason
$pickupPipeline = DirectArrayPipeline $pickupAssignment $pickupDerivationReason
if ($pickupPipeline.PipelineElements.Count -ne 2 -or
    $pickupPipeline.PipelineElements[0] -isnot
        [System.Management.Automation.Language.CommandExpressionAst] -or
    $pickupPipeline.PipelineElements[0].Expression -isnot
        [System.Management.Automation.Language.VariableExpressionAst] -or
    $pickupPipeline.PipelineElements[0].Expression.Splatted -or
    $pickupPipeline.PipelineElements[0].Expression.VariablePath.UserPath -ne
        'ordinaryIds' -or
    $pickupPipeline.PipelineElements[1] -isnot
        [System.Management.Automation.Language.CommandAst]) {
    Fail $pickupDerivationReason
}
$whereCommand = $pickupPipeline.PipelineElements[1]
if ($whereCommand.GetCommandName() -ne 'Where-Object' -or
    $whereCommand.InvocationOperator -ne
        [System.Management.Automation.Language.TokenKind]::Unknown -or
    @($whereCommand.Redirections).Count -ne 0 -or
    $whereCommand.CommandElements.Count -ne 2 -or
    $whereCommand.CommandElements[1] -isnot
        [System.Management.Automation.Language.ScriptBlockExpressionAst]) {
    Fail $pickupDerivationReason
}
$predicateScriptBlock = $whereCommand.CommandElements[1].ScriptBlock
if ($predicateScriptBlock.ParamBlock -ne $null -or
    $predicateScriptBlock.DynamicParamBlock -ne $null -or
    $predicateScriptBlock.BeginBlock -ne $null -or
    $predicateScriptBlock.ProcessBlock -ne $null -or
    (OptionalAstTruthy $predicateScriptBlock 'CleanBlock') -or
    (OptionalAstTruthy $predicateScriptBlock 'Attributes') -or
    (OptionalAstTruthy $predicateScriptBlock 'UsingStatements') -or
    @($predicateScriptBlock.EndBlock.Traps | Where-Object {
        $null -ne $_
    }).Count -ne 0) {
    Fail $pickupDerivationReason
}
$predicateStatements = @($predicateScriptBlock.EndBlock.Statements)
if ($predicateStatements.Count -ne 1 -or
    $predicateStatements[0] -isnot [System.Management.Automation.Language.PipelineAst] -or
    $predicateStatements[0].PipelineElements.Count -ne 1 -or
    $predicateStatements[0].PipelineElements[0] -isnot
        [System.Management.Automation.Language.CommandExpressionAst] -or
    $predicateStatements[0].PipelineElements[0].Expression -isnot
        [System.Management.Automation.Language.BinaryExpressionAst]) {
    Fail $pickupDerivationReason
}
if (OptionalAstTruthy $predicateStatements[0] 'Background') {
    Fail $pickupDerivationReason
}
$pickupPredicate = $predicateStatements[0].PipelineElements[0].Expression
if ($pickupPredicate.Operator -ne
        [System.Management.Automation.Language.TokenKind]::Ine -or
    $pickupPredicate.Left -isnot
        [System.Management.Automation.Language.VariableExpressionAst] -or
    $pickupPredicate.Left.Splatted -or
    $pickupPredicate.Left.VariablePath.UserPath -ne '_' -or
    (MemberBinding $pickupPredicate.Right) -ne '$pickup.pickup_item_id') {
    Fail $pickupDerivationReason
}

$pickupRequire = TopLevelRequire $ast "Require-Condition((Compare-Object`$expectedPickupSnapshot`$pickupSnapshot).Count-eq0)-Message'pickupremaininggroundIDsdonotconservetheexactoriginalset'" 'Stage11D loot evidence guard requires exact pickup conservation in a top-level executable Require command'
RequireImmediatelyAfter $ast $pickupAssignment $pickupRequire 'Stage11D loot evidence guard requires pickup conservation derivation immediately before its Require'

Write-Output 'Stage11D loot validator AST guard passed'
