// src/pages/Home.jsx
import React from 'react';
import '../styles/Home.css';
import uniflash from '../assets/uniflash logo.png';
import yes from '../assets/yes logo.png';
import ti3d from '../assets/3D TI logo.png';
import tiworkstation from '../assets/TI WORKSTATION.png';

export default function Home() {
  return (
    <div className="home-container">
      <div className="home-top-section">
        <div className="home-left-images">
          <div className="image-wrapper top-left-1">
            <img src={uniflash} alt="Feature 1" className="home-image" />
          </div>
        </div>
        
        <div className="home-right-image">
          <div className="image-wrapper top-right">
            <img src={yes} alt="Feature 2" className="home-image" />
          </div>
        </div>
      </div>
      
      <div className="home-bottom-section">
        <div className="image-wrapper center-1">
          <img src={ti3d} alt="Feature 3" className="home-image" />
        </div>
        <div className="image-wrapper center-2">
          <img src={tiworkstation} alt="Feature 4" className="home-image" />
        </div>
      </div>
    </div>
  );
}
