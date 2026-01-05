import React from 'react';
import { CoherenceDashboard } from './CoherenceDashboard';

export const DailyWellness: React.FC = () => {
	return (
		<div style={{ padding: 16 }}>
			<h2 style={{ marginBottom: 12 }}>Real-Time Autonomic View</h2>
			<CoherenceDashboard />
		</div>
	);
};