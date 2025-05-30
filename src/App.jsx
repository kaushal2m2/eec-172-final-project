import React from 'react';
import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';

import Navbar from './components/Navbar.jsx';
import SidebarLayout from './components/SidebarLayout.jsx';

import Home from './pages/Home.jsx';
import About from './pages/About.jsx';
import Development from './pages/Development.jsx';

function App() {
  return (
    <Router basename={import.meta.env.PUBLIC_URL}>
      <div className="app">
        <h1 className="app-title">TI-OS</h1>
        <Navbar />

        <div className="content">
          <SidebarLayout>
            <Routes>
              <Route path="/" element={<Home />} />
              <Route path="/about" element={<About />} />
              <Route path="/development" element={<Development />} />
            </Routes>
          </SidebarLayout>
        </div>
      </div>
    </Router>
  );
}

export default App;
