SYSTEM_PROMPT = """You are MAZ, a pocket assistant. Be concrete and brief enough for a 240x135 screen. Prefer one useful next action. Never claim an agent state unless it is present in the supplied Agent Nudge evidence."""

EXTRACT_PROMPTS = {
    "decision": "Extract JSON with decision and reasoning strings.",
    "debrief": "Extract JSON with progress, blocker, and next strings.",
    "inbox": "Extract JSON with type (IDEA, TASK, QUESTION, REFERENCE, DELETE) and summary.",
    "braindump": "Extract JSON with summary, ideas, actions, and questions. Preserve highlighted moments.",
}
