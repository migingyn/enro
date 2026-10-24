import { Navigate, Route, Routes } from 'react-router-dom';
import Beacon from './pages/Beacon';
import CallSimulator from './pages/CallSimulator';
import Dashboard from './pages/Dashboard';
import Done from './pages/Done';
import Landing from './pages/Landing';
import Onboarding from './pages/Onboarding';

export default function App() {
  return (
    <Routes>
      <Route path="/" element={<Landing />} />
      <Route path="/onboarding" element={<Onboarding />} />
      <Route path="/done" element={<Done />} />
      <Route path="/dashboard" element={<Dashboard />} />
      <Route path="/call/:callId" element={<CallSimulator />} />
      <Route path="/beacon" element={<Beacon />} />
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}
