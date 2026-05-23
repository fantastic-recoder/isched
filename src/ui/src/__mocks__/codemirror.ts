// Mock for CodeMirror 6 and cm6-graphql in Jest tests.
// The QueryEditorComponent wraps CodeMirror in a way that requires the DOM;
// unit tests treat the editor as a black-box and mock this dependency.

const facet = { of: jest.fn(() => ({})) };

export class EditorView {
  static updateListener = facet;

  state = { doc: { toString: () => '', length: 0 } };
  dom = document.createElement('div');
  constructor(_config?: unknown) {}
  dispatch = jest.fn();
  destroy = jest.fn();
}

export class EditorState {
  doc = { toString: () => '', length: 0 };
  static create = jest.fn(() => new EditorState());
}

export const basicSetup: unknown[] = [];

export function graphql(): unknown {
  return {};
}

// Re-export anything else callers may import
export default {};


