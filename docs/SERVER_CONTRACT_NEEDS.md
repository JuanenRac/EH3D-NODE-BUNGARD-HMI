# What the screen needs from the EH3D server

The screen is one more client of EH3D-SERVER. This page lists what it sends and reads beyond the first version of the interface. Everything here is optional on the server's side: a
server that does not know a field keeps working, and the screen falls back to the behaviour of before and says so.

## The bar of the tool changer (`rack`)

On a machine whose model says ATC, `POST /api/v1/machines/{machine}/jobs` (for `"kind": "drill"` and `"kind": "route"`) may carry the places of the tools and the chaining of a
drilling and a routing job. A request without a bar is exactly the one of the first version and has neither field. The setting `rack_chain = off` sends none.

```json
{
  "kind": "route",
  "request_version": 2,
  "rack": {
    "first": 3,
    "carry_in": true,
    "carry_out": false,
    "slots": [ { "tool": 1, "place": 4 }, { "tool": 2, "place": 5 } ],
    "carry_slot": 3
  }
}
```

| Field | Meaning |
|---|---|
| `request_version` | `2` when `rack` is present |
| `rack.first` | how many places of the bar the tools of the jobs before this one take (0 for the first job); the tools of this job without a place of their own wait from the next place |
| `rack.carry_in` | the chuck comes holding the last tool of the job before: the first change of this job takes it to its place and picks the first tool of this one |
| `rack.carry_out` | this job leaves its last tool in the chuck for the next one |
| `rack.slots` | the place of each tool of this job, in the order the job uses them: the tool number (the pen of a routing job) and the place of the bar, 1 to 14 |
| `rack.carry_slot` | with `carry_in`, the place of the tool the chuck comes holding |

What the server does: plan the tool changes with the places and the chaining; refuse the job with 400 or 422 when a place is not defined in the machine or is out of reach (and say
which); answer the job with `"rack_applied": true` only when it planned with them.

What the screen does: on 400 or 422 it sends the job again without the field and shows a red line; on 200 without `rack_applied` it believes the server, so the next program is
not sent as a chained one, and the red line says that the tools must be in the bar in the order of the job.

## What the screen reads of a running job

All of these are optional; a missing one is a dash on the card of the work in hand.

| Where | Field | Used for |
|---|---|---|
| fleet, top level | `time_s` | the clock of the server in seconds since 1970, for the hour the job is expected to end at when the screen has no clock of its own |
| `machines[].job` | `position` `{x, y, z}` | the position of the head in mm, Z positive downwards |
| `machines[].job` | `feed`, `spindle_rpm` | the feed in mm per minute and the turns of the spindle |
| `machines[].job` | `depth_mm` | how deep the stroke is |
| `machines[].job` | `tool_changes`, `elapsed_s`, `routed_mm` | the counters and the time elapsed |
| `machines[].job` | `tool_slot` | the place of the bar of the tool at work |

## What the screen cannot do yet

The moves of the panel to move the machine by hand have no route on the server: they wait for the link to the node. When that link exists, the orders are relative moves
(`dx`, `dy`, `dz` in mm, Z positive downwards, and a feed in mm per minute); the screen already works them out and passes them through the gate.
