import React from 'react';
import { Provider } from 'react-redux';
import { render, screen } from '@testing-library/react';
import App from './App';
import { store } from './Actions';

jest.mock('./PlayModule', () => ({
  PlayModule: {
    HEAPU8: { byteLength: 64 * 1024 * 1024 },
    clearStats: jest.fn(),
    getFrames: jest.fn(() => 60),
    getJitBlocksCompiled: jest.fn(() => 12),
    getJitBlocksLive: jest.fn(() => 8),
    getJitCodeBytes: jest.fn(() => 1024),
    getJitCompileMs: jest.fn(() => 4),
    setEeFreqScale: jest.fn(),
    setFrameLimit: jest.fn(),
  },
  initPlayModule: jest.fn(() => Promise.resolve()),
}));

test('renders the isolated performance laboratory', async () => {
  render(<Provider store={store}><App /></Provider>);
  expect(await screen.findByRole('heading', { name: 'Web performance lab' })).toBeInTheDocument();
  expect(screen.getByText('Runtime health')).toBeInTheDocument();
  expect(screen.getByText('Reconnect runtime')).toBeInTheDocument();
  expect(screen.getByText('Branch: codex/web-perf-lab')).toBeInTheDocument();
});
