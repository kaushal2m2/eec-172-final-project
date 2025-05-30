// src/pages/Home.jsx
import React from 'react';
import homeImg from '../assets/home.png';

export default function Home() {
  return (
    <div className="home-hero">
      {/* full-width image */}
      <img src={homeImg} alt="TI-OS hero" className="home-hero__img"/>

      {/* overlayed heading (optional) */}
      {/* <h2 className="home-hero__title">Welcome to TI-OS</h2> */}
    </div>
  );
}
